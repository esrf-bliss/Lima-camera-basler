//###########################################################################
// This file is part of LImA, a Library for Image Acquisition
//
// Copyright (C) : 2009-2011
// European Synchrotron Radiation Facility
// BP 220, Grenoble 38043
// FRANCE
//
// This is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//###########################################################################
#ifdef WIN32
#include <winsock2.h>
#endif

#include <sstream>
#include <iostream>
#include <string>
#include <algorithm>
#include <math.h>
#include "BaslerCamera.h"
#include "BaslerVideoCtrlObj.h"

using namespace lima;
using namespace lima::Basler;
using namespace std;

#if !defined(WIN32)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define min(A,B) std::min(A,B)
#define max(A,B) std::max(A,B)
#endif

const static int DEFAULT_TIME_OUT = 600000; // 10 minutes
const static int DEFAULT_HEARTBEAT_TIMEOUT = 1000; //(1s)

const static std::string IP_PREFIX = "ip://";
const static std::string SN_PREFIX = "sn://";
const static std::string UNAME_PREFIX = "uname://";

//---------------------------
//- utility function
//---------------------------
static inline const char* _get_ip_addresse(const char *name_ip)
{
  
  if(inet_addr(name_ip) != INADDR_NONE)
    return name_ip;
  else
    {
      struct hostent *host = gethostbyname(name_ip);
      if(!host)
    {
      char buffer[256];
      snprintf(buffer,sizeof(buffer),"Can not found ip for host %s ",name_ip);
      throw LIMA_HW_EXC(Error,buffer);
    }
      return inet_ntoa(*((struct in_addr*)host->h_addr));
    }
}
//---------------------------
//- Basler lock utility
//---------------------------
Camera::Basler::Basler(const Camera::Basler& other) :
  camera(other.camera),
  _camera(other._camera)
{
  AutoMutex lock(_camera.m_cond.mutex());
  ++_camera.m_basler_inuse;
}

Camera::Basler::Basler(Camera& cam,Camera_t* camera) :
  camera(*camera),
  _camera(cam)
{
  ++_camera.m_basler_inuse;
}

Camera::Basler::~Basler()
{
  AutoMutex lock(_camera.m_cond.mutex());
  --_camera.m_basler_inuse;
  _camera.m_cond.broadcast();
}

//---------------------------
//- utility thread
//---------------------------

class Camera::_AcqThread : public Thread
{
    DEB_CLASS_NAMESPC(DebModCamera, "Camera", "_AcqThread");
    public:
        _AcqThread(Camera &aCam);
    virtual ~_AcqThread();
    
    protected:
        virtual void threadFunction();
    
    private:
        Camera&    m_cam;
};


//---------------------------
//- Ctor
//---------------------------
Camera::Camera(const std::string& camera_id,int packet_size,int receive_priority)
        : m_nb_frames(1),
          m_status(Ready),
          m_wait_flag(true),
          m_quit(false),
          m_thread_running(true),
          m_image_number(0),
          m_exp_time(1.),
          m_timeout(DEFAULT_TIME_OUT),
          m_latency_time(0.),
          m_socketBufferSize(0),
	  m_camera_id(camera_id),
	  m_packet_size(packet_size),
	  m_camera(NULL),
	  m_basler_inuse(0),
          StreamGrabber_(NULL),
	  m_acq_thread(NULL),
          m_receive_priority(receive_priority),
	  m_video_flag_mode(false),
	  m_video(NULL)
{
    DEB_CONSTRUCTOR();

    // by default use ip:// scheme if none is given
    if (m_camera_id.find("://") == std::string::npos)
    {
        m_camera_id = "ip://" + m_camera_id;
    }

    m_device_info.SetDeviceClass(m_camera->DeviceClass());

    if(!m_camera_id.compare(0, IP_PREFIX.size(), IP_PREFIX))
    {
        // m_camera_id is not really necessarily an IP, it may also be a DNS name
	Pylon::String_t camera_ip(_get_ip_addresse(m_camera_id.substr(IP_PREFIX.size()).c_str()));
	m_device_info.SetPropertyValue("IpAddress", camera_ip);
    }
    else if (!m_camera_id.compare(0, SN_PREFIX.size(), SN_PREFIX))
    {
        Pylon::String_t serial_number(m_camera_id.substr(SN_PREFIX.size()).c_str());
	m_device_info.SetSerialNumber(serial_number);
    }
    else if(!m_camera_id.compare(0, UNAME_PREFIX.size(), UNAME_PREFIX))
    {
        Pylon::String_t user_name(m_camera_id.substr(UNAME_PREFIX.size()).c_str());
        m_device_info.SetUserDefinedName(user_name);
    }
    else
    {
        THROW_CTL_ERROR(InvalidValue) << "Unrecognized camera id: " << camera_id;
    }

    WaitObject_ = WaitObjectEx::Create();
    for(int i = 0;i < NB_COLOR_BUFFER;++i)
      m_color_buffer[i] = NULL;

    m_acq_thread = new _AcqThread(*this);
    m_acq_thread->start();
}

// Returns a ready to use (attached & opened) reference to basler camera
// If an error occurs during attach/open an exception is thrown
Camera::Basler Camera::_getBasler()
{
    DEB_MEMBER_FUNCT();

    AutoMutex lock(m_cond.mutex());
    if (m_camera)
    {
      return Basler(*this,m_camera);
    }

    try
    {
        m_camera = new Camera_t;
        Pylon::CTlFactory& TlFactory = Pylon::CTlFactory::GetInstance();
	DEB_TRACE() << "Creating the Pylon device attached to: " << DEB_VAR1(m_camera_id);
        Pylon::IPylonDevice* device = TlFactory.CreateDevice(m_device_info);

	if (!device)
	{
	    THROW_HW_ERROR(Error) << "Unable to find camera with selected ID!";
	}

        DEB_TRACE() << "Attaching device to camera...";
        m_camera->Attach(device);

	const Pylon::CDeviceInfo &dev_info = m_camera->GetDeviceInfo();

	DEB_TRACE() << "Basler attached:";
	DEB_TRACE() << "Vendor        = " << dev_info.GetVendorName();
	DEB_TRACE() << "Model         = " << dev_info.GetModelName();
	DEB_TRACE() << "Class         = " << dev_info.GetDeviceClass();
	DEB_TRACE() << "Version       = " << dev_info.GetDeviceVersion();
	DEB_TRACE() << "User Name     = " << dev_info.GetUserDefinedName();
	DEB_TRACE() << "Friendly Name = " << dev_info.GetFriendlyName();
	DEB_TRACE() << "Full Name     = " << dev_info.GetFullName();
	DEB_TRACE() << "Serial nb.    = " << dev_info.GetSerialNumber();

        DEB_TRACE() << "Opening camera...";
        m_camera->Open();

	GenApi::CIntegerPtr timeout_ptr = m_camera->GetTLNodeMap()->GetNode("HeartbeatTimeout");
	if(timeout_ptr.IsValid())
	{
	    DEB_TRACE() << "Setting heartbeat timeout...";
	    int timeout = DEFAULT_HEARTBEAT_TIMEOUT;
	    timeout -= (timeout % timeout_ptr->GetInc());
	    timeout_ptr->SetValue(timeout);
	}

	DEB_TRACE() << "Registering removal callback... ";
	m_callback_handle = Pylon::RegisterRemovalCallback(device, *this,
							   &Camera::_onRemoval);

        if(m_packet_size > 0)
	{
	    m_camera->GevSCPSPacketSize.SetValue(m_packet_size);
        }

        // Set the image format and AOI
        DEB_TRACE() << "Set the image format and AOI";
        static const char* PixelFormatStr[] = {"BayerRG16","BayerBG16",
					       "BayerRG12","BayerBG12",
					       "BayerRG8","BayerBG8",
					       "Mono16", "Mono12", "Mono8",NULL};
        bool formatSetFlag = false;
        for(const char** pt = PixelFormatStr;*pt;++pt)
        {

            GenApi::IEnumEntry *anEntry = m_camera->PixelFormat.GetEntryByName(*pt);
            if(anEntry && GenApi::IsAvailable(anEntry))
            {
                formatSetFlag = true;
		m_color_flag = *pt[0] == 'B';
		m_camera->PixelFormat.SetIntValue(anEntry->GetValue());
                DEB_TRACE() << "Set pixel format to " << *pt;
                break;
            }
        }
        if(!formatSetFlag)
            THROW_HW_ERROR(Error) << "Unable to set PixelFormat for the camera!";

        DEB_TRACE() << "Set the ROI to full frame";
	if(_isRoiAvailable(*m_camera))
	{
	    Roi aFullFrame(0, 0, m_camera->WidthMax(), m_camera->HeightMax());
	    _setRoi(*m_camera, aFullFrame);
	}
        // Set Binning to 1, only if the camera has this functionality
        if (_isBinningAvailable(*m_camera))
        {
            DEB_TRACE() << "Set BinningH & BinningV to 1";
            m_camera->BinningVertical.SetValue(1);
            m_camera->BinningHorizontal.SetValue(1);
        }

        // Set the camera to continuous frame mode
        DEB_TRACE() << "Set the camera to continuous frame mode";
        m_camera->TriggerSelector.SetValue(TriggerSelector_AcquisitionStart);
        m_camera->AcquisitionMode.SetValue(AcquisitionMode_Continuous);

        if ( GenApi::IsAvailable(m_camera->ExposureAuto ))
        {
            DEB_TRACE() << "Set ExposureAuto to Off";
            m_camera->ExposureAuto.SetValue(ExposureAuto_Off);
        }

	// Start with internal trigger
	_setTrigMode(*m_camera, IntTrig);
        // Get the image buffer size
        DEB_TRACE() << "Get the image buffer size";
        ImageSize_ = (size_t)(m_camera->PayloadSize.GetValue());
    }
    catch (GenICam::GenericException &e)
    {
        delete m_camera;
        m_camera = NULL;
        m_callback_handle = NULL;
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }

    if(m_color_flag)
        _initColorStreamGrabber();

    return Basler(*this,m_camera);
}

//---------------------------
//- Dtor
//---------------------------
Camera::~Camera()
{
    DEB_DESTRUCTOR();
    try
    {
        // Close camera
	_disconnect();

        // Stop Acq thread
        delete m_acq_thread;
        m_acq_thread = NULL;
        
	if (m_video_flag_mode)
	  for(int i = 0;i < NB_COLOR_BUFFER;++i)
#ifdef __unix
	    free(m_color_buffer[i]);
#else
	_aligned_free(m_color_buffer[i]);
#endif
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
}

void Camera::prepareAcq()
{
    DEB_MEMBER_FUNCT();
    m_image_number=0;

    if(m_video_flag_mode)
      return;			// Nothing to do if color camera

    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
	_freeStreamGrabber();
        // Get the first stream grabber object of the selected camera
        DEB_TRACE() << "Get the first stream grabber object of the selected camera";
        StreamGrabber_ = new Camera_t::StreamGrabber_t(camera.GetStreamGrabber(0));
	//Change priority to m_receive_priority
	if(m_receive_priority > 0)
	  {
	    StreamGrabber_->ReceiveThreadPriorityOverride.SetValue(true);
	    StreamGrabber_->ReceiveThreadPriority.SetValue(m_receive_priority);
	  }
        // Set Socket Buffer Size
        DEB_TRACE() << "Set Socket Buffer Size";
        if (m_socketBufferSize >0 )
        {
            StreamGrabber_->SocketBufferSize.SetValue(m_socketBufferSize);
          }
        // Open the stream grabber
        DEB_TRACE() << "Open the stream grabber";
        StreamGrabber_->Open();
        if(!StreamGrabber_->IsOpen())
        {
            delete StreamGrabber_;
            StreamGrabber_ = NULL;
            THROW_HW_ERROR(Error) << "Unable to open the steam grabber!";
        }
        // We won't use image buffers greater than ImageSize
        DEB_TRACE() << "We won't use image buffers greater than ImageSize";
        StreamGrabber_->MaxBufferSize.SetValue((const size_t)ImageSize_);

        StdBufferCbMgr& buffer_mgr = m_buffer_ctrl_obj.getBuffer();
        // We won't queue more than c_nBuffers image buffers at a time
        int nb_buffers;
        buffer_mgr.getNbBuffers(nb_buffers);
        DEB_TRACE() << "We'll queue " << nb_buffers << " image buffers";
        StreamGrabber_->MaxNumBuffer.SetValue(nb_buffers);

        // Allocate all resources for grabbing. Critical parameters like image
        // size now must not be changed until FinishGrab() is called.
        DEB_TRACE() << "Allocate all resources for grabbing, PrepareGrab";
        StreamGrabber_->PrepareGrab();

        // Put buffer into the grab queue for grabbing
        DEB_TRACE() << "Put buffer into the grab queue for grabbing";
        for(int i = 0;i < nb_buffers;++i)
        {
            void *ptr = buffer_mgr.getFrameBufferPtr(i);
            // The registration returns a handle to be used for queuing the buffer.
            StreamBufferHandle bufferId = StreamGrabber_->RegisterBuffer(ptr,(const size_t)ImageSize_);
            StreamGrabber_->QueueBuffer(bufferId, NULL);
        }
	if(m_trigger_mode == IntTrigMult)
	  _startAcq();
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
}
//---------------------------
//- Camera::start()
//---------------------------
void Camera::startAcq()
{
    DEB_MEMBER_FUNCT();
    try
    {
	if(!m_image_number)
	  {
	    if(m_video)
	      m_video->getBuffer().setStartTimestamp(Timestamp::now());
	    else
	      m_buffer_ctrl_obj.getBuffer().setStartTimestamp(Timestamp::now());
	  }
	if(m_trigger_mode == IntTrigMult)
	  {
	    Basler basler_cam = this->_getBasler();
	    Camera_t& camera = basler_cam.camera;
	    camera.TriggerSoftware.Execute();
	  }
	else
	_startAcq();
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
}

void Camera::_startAcq()
{
  DEB_MEMBER_FUNCT();
  Basler basler_cam = this->_getBasler();
  Camera_t& camera = basler_cam.camera;
  camera.AcquisitionStart.Execute();

  //Start acqusition thread
  AutoMutex aLock(m_cond.mutex());
  m_wait_flag = false;
  m_cond.broadcast();
}
//---------------------------
//- Camera::stopAcq()
//---------------------------
void Camera::stopAcq()
{
  _stopAcq(false);
}

//---------------------------
//- Camera::_stopAcq()
//---------------------------
void Camera::_stopAcq(bool internalFlag)
{
  DEB_MEMBER_FUNCT();
    try
    {
        AutoMutex aLock(m_cond.mutex());
        if(m_status != Camera::Ready)
        {
            while(!internalFlag && m_thread_running)
            {
                m_wait_flag = true;
                WaitObject_.Signal();
                m_cond.wait();
            }
            aLock.unlock();

            //Let the acq thread stop the acquisition
            if(!internalFlag) return;
            
            // Stop acquisition
            DEB_TRACE() << "Stop acquisition";
	    Basler basler_cam = this->_getBasler();
	    Camera_t& camera = basler_cam.camera;
	    camera.AcquisitionStop.Execute();
	    if(!m_video_flag_mode)
	      _freeStreamGrabber();
	    _setStatus(Camera::Ready,false);
        }
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }    
}

void Camera::_onRemoval(Pylon::IPylonDevice* device)
{
    DEB_MEMBER_FUNCT();
    const Pylon::CDeviceInfo& dev_info = device->GetDeviceInfo();
    DEB_TRACE() << "Device '" << dev_info.GetFriendlyName() << "' removed!";
    _disconnect();
  }

void Camera::_disconnect()
{
    DEB_MEMBER_FUNCT();

    AutoMutex lock(m_cond.mutex());
    m_wait_flag = true;		// stop acquisition thread
    WaitObject_.Signal();
    while(m_thread_running)
      m_cond.wait();

    _freeStreamGrabber();
    m_status = Camera::Ready;

    while(m_basler_inuse > 0)
      m_cond.wait();

    Camera_t* camera = m_camera;
    Pylon::DeviceCallbackHandle handle = m_callback_handle;
    m_camera = NULL;
    m_callback_handle = NULL;

    if (!camera)
        return;

    if (camera->IsOpen())
    {
        if (handle)
        {
            DEB_TRACE() << "Unregistering removal callback...";
            camera->DeregisterRemovalCallback(handle);
        }
        DEB_TRACE() << "Closing...";
        camera->Close();
    }
    delete camera;
}

void Camera::_freeStreamGrabber()
{
  DEB_MEMBER_FUNCT();
  if(StreamGrabber_)
    {
      // Get the pending buffer back (You are not allowed to deregister
      // buffers when they are still queued)
      StreamGrabber_->CancelGrab();
    
      // Get all buffers back
      for (GrabResult r; StreamGrabber_->RetrieveResult(r););
    
      // Free all resources used for grabbing
      DEB_TRACE() << "Free all resources used for grabbing";
      StreamGrabber_->FinishGrab();
      StreamGrabber_->Close();
      delete StreamGrabber_;
      StreamGrabber_ = NULL;         
    }
}

void Camera::_initColorStreamGrabber()
{
  DEB_MEMBER_FUNCT();
  Basler basler_cam = this->_getBasler();
  Camera_t& camera = basler_cam.camera;
  StreamGrabber_ = new Camera_t::StreamGrabber_t(camera.GetStreamGrabber(0));
  StreamGrabber_->Open();
  if(!StreamGrabber_->IsOpen())
    {
      delete StreamGrabber_;
      StreamGrabber_ = NULL;
      THROW_HW_ERROR(Error) << "Unable to open the steam grabber!";
    }
  StreamGrabber_->MaxBufferSize.SetValue((const size_t)ImageSize_);
  StreamGrabber_->MaxNumBuffer.SetValue(NB_COLOR_BUFFER);
  StreamGrabber_->PrepareGrab();

  for(int i = 0;i < NB_COLOR_BUFFER;++i)
    {
      if(!m_color_buffer[i])
#ifdef __unix
	posix_memalign(&m_color_buffer[i],16,ImageSize_);
#else
        m_color_buffer[i] = _aligned_malloc(ImageSize_,16);
#endif
      StreamBufferHandle bufferId = StreamGrabber_->RegisterBuffer(m_color_buffer[i],
								   (const size_t)ImageSize_);
      StreamGrabber_->QueueBuffer(bufferId,NULL);
    }
  m_video_flag_mode = true;
}

//---------------------------
//- Camera::_AcqThread::threadFunction()
//---------------------------
void Camera::_AcqThread::threadFunction()
{
  DEB_MEMBER_FUNCT();
  AutoMutex aLock(m_cam.m_cond.mutex());
  StdBufferCbMgr& buffer_mgr = m_cam.m_buffer_ctrl_obj.getBuffer();

    while(!m_cam.m_quit)
    {
        while(m_cam.m_wait_flag && !m_cam.m_quit)
        {
          DEB_TRACE() << "Wait";
          m_cam.m_thread_running = false;
          m_cam.m_cond.broadcast();
          m_cam.m_cond.wait();
        }
        DEB_TRACE() << "Run";
        m_cam.m_thread_running = true;
        if(m_cam.m_quit) return;
    
        m_cam.m_status = Camera::Exposure;
        m_cam.m_cond.broadcast();
        aLock.unlock();

        try
        {
            WaitObjects waitset;
            waitset.Add(m_cam.WaitObject_);
            waitset.Add(m_cam.StreamGrabber_->GetWaitObject());
    
            bool continueAcq = true;
            while(continueAcq && (!m_cam.m_nb_frames || m_cam.m_image_number < m_cam.m_nb_frames))
            {
	      m_cam._setStatus(Camera::Exposure,false);
                unsigned int event_number;
                if(waitset.WaitForAny(m_cam.m_timeout,&event_number)) // Wait m_timeout
                {
                    switch(event_number)
                    {
                        case 0:    // event
                            DEB_TRACE() << "Receive Event";
                            m_cam.WaitObject_.Reset();
                            aLock.lock();
                            continueAcq = !m_cam.m_wait_flag && !m_cam.m_quit;
                            aLock.unlock();
                        break;
                        case 1:
                            // Get the grab result from the grabber's result queue
                            GrabResult Result;
                            m_cam.StreamGrabber_->RetrieveResult(Result);
                            if (Grabbed == Result.Status())
                            {
                                // Grabbing was successful, process image
                                m_cam._setStatus(Camera::Readout,false);
                                DEB_TRACE()  << "image#" << DEB_VAR1(m_cam.m_image_number) <<" acquired !";
				if(!m_cam.m_video_flag_mode)
				  {
				    int nb_buffers;
				    buffer_mgr.getNbBuffers(nb_buffers);
				    if (!m_cam.m_nb_frames || 
					m_cam.m_image_number < int(m_cam.m_nb_frames - nb_buffers))
				      m_cam.StreamGrabber_->QueueBuffer(Result.Handle(),NULL);
                                
				    HwFrameInfoType frame_info;
				    frame_info.acq_frame_nb = m_cam.m_image_number;
				    continueAcq = buffer_mgr.newFrameReady(frame_info);
				    DEB_TRACE() << DEB_VAR1(continueAcq);
				  }
				else
				  {
				    m_cam.StreamGrabber_->QueueBuffer(Result.Handle(),NULL);
				    VideoMode mode;
				    switch(Result.GetPixelType())
				      {
				      case PixelType_Mono8:		mode = Y8;		break;
				      case PixelType_Mono10: 		mode = Y16;		break;
				      case PixelType_Mono12:  		mode = Y16;		break;
				      case PixelType_Mono16:  		mode = Y16;		break;
				      case PixelType_BayerRG8:  	mode = BAYER_RG8;	break;
				      case PixelType_BayerBG8: 		mode = BAYER_BG8;	break;  
				      case PixelType_BayerRG10:  	mode = BAYER_RG16;	break;
				      case PixelType_BayerBG10:    	mode = BAYER_BG16;	break;
				      case PixelType_BayerRG12:    	mode = BAYER_RG16;	break;
				      case PixelType_BayerBG12:      	mode = BAYER_BG16;	break;
				      case PixelType_RGB8packed:  	mode = RGB24;		break;
				      case PixelType_BGR8packed:  	mode = BGR24;		break;
				      case PixelType_RGBA8packed:  	mode = RGB32;		break;
				      case PixelType_BGRA8packed:  	mode = BGR32;		break;
				      case PixelType_YUV411packed:  	mode = YUV411;		break;
				      case PixelType_YUV422packed:  	mode = YUV422;		break;
				      case PixelType_YUV444packed:  	mode = YUV444;		break;
				      case PixelType_BayerRG16:    	mode = BAYER_RG16;	break;
				      case PixelType_BayerBG16:    	mode = BAYER_BG16;	break;
				      default:
					DEB_ERROR() << "Image type not managed";
					return;
				      }
				    m_cam.m_video->callNewImage((char*)Result.Buffer(),
								Result.GetSizeX(),
								Result.GetSizeY(),
								mode);
				  }
                                ++m_cam.m_image_number;
                            }
                            else if (Failed == Result.Status())
                            {
                                // Error handling
                                DEB_ERROR() << "No image acquired!"
                                            << " Error code : 0x"
                                            << DEB_VAR1(hex)<< " "
                                            << Result.GetErrorCode()
                                            << " Error description : "
                                            << Result.GetErrorDescription();
                                
                                if(!m_cam.m_nb_frames) //Do not stop acquisition in "live" mode, just IGNORE  error
                                {
                                    m_cam.StreamGrabber_->QueueBuffer(Result.Handle(), NULL);
                                }
                                else            //in "snap" mode , acquisition must be stopped
                                {
                                    m_cam._setStatus(Camera::Fault,false);
                                    continueAcq = false;
                                }
                            }
                        break;
                    }
                }
                else
                {
                    // Timeout
                    DEB_ERROR() << "Timeout occurred!";
                    m_cam._setStatus(Camera::Fault,false);
                    continueAcq = false;
                }
            }
            m_cam._stopAcq(true);
        }
        catch (GenICam::GenericException &e)
        {
            // Error handling
            DEB_ERROR() << "GeniCam Error! "<< e.GetDescription();
        }
	catch(Exception)
	{
	}
        aLock.lock();
        m_cam.m_wait_flag = true;
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
Camera::_AcqThread::_AcqThread(Camera &aCam) :
                    m_cam(aCam)
{
    pthread_attr_setscope(&m_thread_attr,PTHREAD_SCOPE_PROCESS);
}
//-----------------------------------------------------
//
//-----------------------------------------------------

Camera::_AcqThread::~_AcqThread()
{
    AutoMutex aLock(m_cam.m_cond.mutex());
    m_cam.m_quit = true;
    m_cam.m_thread_running = false;
    m_cam.WaitObject_.Signal();
    m_cam.m_cond.broadcast();
    aLock.unlock();
    
    join();
}

//-----------------------------------------------------
//
//-----------------------------------------------------

void Camera::getDetectorImageSize(Size& size)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    size = Size(camera.WidthMax(), camera.HeightMax());
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getImageType(ImageType& type)
{
    DEB_MEMBER_FUNCT();
    try
    {
        Basler basler_cam = this->_getBasler();
	Camera_t& camera = basler_cam.camera;
        PixelFormatEnums ps = camera.PixelFormat.GetValue();
        switch( ps )
        {
            case PixelFormat_Mono8:
                type= Bpp8;
            break;
              
            case PixelFormat_Mono12:
                type= Bpp12;
            break;
              
            case PixelFormat_Mono16: //- this is in fact 12 bpp inside a 16bpp image
                type= Bpp16;
            break;
              
            default:
                type= Bpp10;
            break;
        }
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setImageType(ImageType type)
{
    DEB_MEMBER_FUNCT();
    try
    {
        Basler basler_cam = this->_getBasler();
	Camera_t& camera = basler_cam.camera;

        switch( type )
        {
            case Bpp8:
                camera.PixelFormat.SetValue(PixelFormat_Mono8);
                break;
            case Bpp12:
                camera.PixelFormat.SetValue(PixelFormat_Mono12);
                break;
            case Bpp16:
                camera.PixelFormat.SetValue(PixelFormat_Mono16);
                break;
              
            default:
                THROW_HW_ERROR(Error) << "Cannot change the format of the camera !";
                break;
        }
    }
    catch (GenICam::GenericException &e)
    {
      // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
}
//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getDetectorType(string& type)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;

    type = camera.GetDeviceInfo().GetVendorName();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getDetectorModel(string& type)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;

    type = camera.GetDeviceInfo().GetModelName();
    return;        
}

//-----------------------------------------------------
//
//-----------------------------------------------------
HwBufferCtrlObj* Camera::getBufferCtrlObj()
{
    return &m_buffer_ctrl_obj;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setTrigMode(TrigMode mode)
{
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    _setTrigMode(camera, mode);
}

void Camera::_setTrigMode(Camera_t& camera, TrigMode mode)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(mode);

    if(mode == m_trigger_mode)
      return;			// Nothing to do

    try
    {        
        GenApi::IEnumEntry *enumEntryFrameStart = camera.TriggerSelector.GetEntryByName("FrameStart");
        if(enumEntryFrameStart && GenApi::IsAvailable(enumEntryFrameStart))
            camera.TriggerSelector.SetValue( TriggerSelector_FrameStart );
        else
            camera.TriggerSelector.SetValue( TriggerSelector_AcquisitionStart );

    	if ( mode == IntTrig )
        {
            //- INTERNAL
            camera.TriggerMode.SetValue( TriggerMode_Off );
            camera.ExposureMode.SetValue(ExposureMode_Timed);
	    camera.AcquisitionFrameRateEnable.SetValue(true);
	}
        else if ( mode == IntTrigMult )
        {
	    camera.TriggerMode.SetValue(TriggerMode_On);
	    camera.TriggerSource.SetValue(TriggerSource_Software);
	    camera.AcquisitionFrameCount.SetValue(1);
	    camera.ExposureMode.SetValue(ExposureMode_Timed);
        }
        else if ( mode == ExtGate )
        {
            //- EXTERNAL - TRIGGER WIDTH
            camera.TriggerMode.SetValue( TriggerMode_On );
	    camera.TriggerSource.SetValue(TriggerSource_Line1);
            camera.AcquisitionFrameRateEnable.SetValue( false );
            camera.ExposureMode.SetValue( ExposureMode_TriggerWidth );
        }        
        else //ExtTrigMult
        {
            camera.TriggerMode.SetValue( TriggerMode_On );
	    camera.TriggerSource.SetValue(TriggerSource_Line1);
            camera.AcquisitionFrameRateEnable.SetValue( false );
            camera.ExposureMode.SetValue( ExposureMode_Timed );
        }
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }

    m_trigger_mode = mode;
}

void Camera::getTrigMode(TrigMode& mode)
{
  DEB_MEMBER_FUNCT();

  AutoMutex aLock(m_cond.mutex());
  mode = m_trigger_mode;
  
  DEB_RETURN() << DEB_VAR1(m_trigger_mode);
}
//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::_readTrigMode()
{
    DEB_MEMBER_FUNCT();
    int frameStart = TriggerMode_Off, acqStart, expMode;
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        acqStart = camera.TriggerMode.GetValue();

        GenApi::IEnumEntry *enumEntryFrameStart = camera.TriggerSelector.GetEntryByName("FrameStart");
        if(enumEntryFrameStart && GenApi::IsAvailable(enumEntryFrameStart))
        {
            camera.TriggerSelector.SetValue( TriggerSelector_FrameStart );
            frameStart =  camera.TriggerMode.GetValue();
        }

        expMode = camera.ExposureMode.GetValue();
    
	if(acqStart == TriggerMode_On)
	  {
	    int source = camera.TriggerSource.GetValue();
	    if(source == TriggerSource_Software)
	      m_trigger_mode = IntTrigMult;
	    else if(expMode == ExposureMode_TriggerWidth)
	      m_trigger_mode = ExtGate;
	    else
	      m_trigger_mode = ExtTrigMult;
	  }
	else
	  m_trigger_mode = IntTrig;
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }        
   	
    DEB_RETURN() << DEB_VAR4(m_trigger_mode,acqStart, frameStart, expMode);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setExpTime(double exp_time)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(exp_time);
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    
    TrigMode mode;
    getTrigMode(mode);
    
    try
    {
        if(mode !=  ExtGate) { // the expTime can not be set in ExtGate!
            if (GenApi::IsAvailable(camera.ExposureTimeBaseAbs))
            {
                //If scout or pilot, exposure time has to be adjusted using
                // the exposure time base + the exposure time raw.
                //see ImageGrabber for more details !!!
                camera.ExposureTimeBaseAbs.SetValue(100.0); //- to be sure we can set the Raw setting on the full range (1 .. 4095)
                double raw = ::ceil(exp_time / 50);
                camera.ExposureTimeRaw.SetValue(static_cast<int> (raw));
                raw = static_cast<double> (camera.ExposureTimeRaw.GetValue());
                camera.ExposureTimeBaseAbs.SetValue(1E6 * (exp_time / raw));
		DEB_TRACE() << "raw = " << raw;
		DEB_TRACE() << "ExposureTimeBaseAbs = " << (1E6 * (exp_time / raw));			
            }
            else
            {
                // More recent model like ACE and AVIATOR support direct programming of the exposure using
                // the exposure time absolute.
                camera.ExposureTimeAbs.SetValue(1E6 * exp_time);
            }
        }
        
        m_exp_time = exp_time;

        // set the frame rate using expo time + latency
        if (m_latency_time < 1e-6) // Max camera speed
        {
            camera.AcquisitionFrameRateEnable.SetValue(false);
        }
        else
        {
            double periode = m_latency_time + m_exp_time;
            camera.AcquisitionFrameRateEnable.SetValue(true);
            camera.AcquisitionFrameRateAbs.SetValue(1 / periode);
            DEB_TRACE() << DEB_VAR1(camera.AcquisitionFrameRateAbs.GetValue());
        }

    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getExpTime(double& exp_time)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        double value = 1.0E-6 * static_cast<double>(camera.ExposureTimeAbs.GetValue());
        exp_time = value;
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }            
    DEB_RETURN() << DEB_VAR1(exp_time);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setLatTime(double lat_time)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(lat_time);
    m_latency_time = lat_time;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getLatTime(double& lat_time)
{
    DEB_MEMBER_FUNCT();
    lat_time = m_latency_time;
    DEB_RETURN() << DEB_VAR1(lat_time);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getExposureTimeRange(double& min_expo, double& max_expo)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        // Pilot and and Scout do not have TimeAbs capability
        if (GenApi::IsAvailable(camera.ExposureTimeBaseAbs))
        {
            // memorize initial value of exposure time
            DEB_TRACE() << "memorize initial value of exposure time";
            int initial_raw = camera.ExposureTimeRaw.GetValue();
            DEB_TRACE() << "initial_raw = " << initial_raw;
            double initial_base = camera.ExposureTimeBaseAbs.GetValue();
            DEB_TRACE() << "initial_base = " << initial_base;

            DEB_TRACE() << "compute Min/Max allowed values of exposure time";
            // fix raw/base in order to get the Max of Exposure            
            camera.ExposureTimeBaseAbs.SetValue(camera.ExposureTimeBaseAbs.GetMax());
            max_expo = 1E-06 * camera.ExposureTimeBaseAbs.GetValue() * camera.ExposureTimeRaw.GetMax();
            DEB_TRACE() << "max_expo = " << max_expo << " (s)";

            // fix raw/base in order to get the Min of Exposure            
            camera.ExposureTimeBaseAbs.SetValue(camera.ExposureTimeBaseAbs.GetMin());
            min_expo = 1E-06 * camera.ExposureTimeBaseAbs.GetValue() * camera.ExposureTimeRaw.GetMin();
            DEB_TRACE() << "min_expo = " << min_expo << " (s)";

            // reload initial value of exposure time
            camera.ExposureTimeBaseAbs.SetValue(initial_base);
            camera.ExposureTimeRaw.SetValue(initial_raw);

            DEB_TRACE() << "initial value of exposure time was reloaded";
        }
        else
        {
            min_expo = camera.ExposureTimeAbs.GetMin()*1e-6;
            max_expo = camera.ExposureTimeAbs.GetMax()*1e-6;
        }
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }


    DEB_RETURN() << DEB_VAR2(min_expo, max_expo);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getLatTimeRange(double& min_lat, double& max_lat)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        min_lat = 0;
        double minAcqFrameRate = camera.AcquisitionFrameRateAbs.GetMin();
        if (minAcqFrameRate > 0)
            max_lat = 1 / minAcqFrameRate;
        else
            max_lat = 0;
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
    DEB_RETURN() << DEB_VAR2(min_lat, max_lat);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setNbFrames(int nb_frames)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(nb_frames);
    m_nb_frames = nb_frames;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getNbFrames(int& nb_frames)
{
    DEB_MEMBER_FUNCT();
    nb_frames = m_nb_frames;
    DEB_RETURN() << DEB_VAR1(nb_frames);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getNbHwAcquiredFrames(int &nb_acq_frames)
{ 
    DEB_MEMBER_FUNCT();    
    nb_acq_frames = m_image_number;
}
  
//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getStatus(Camera::Status& status)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    AutoMutex aLock(m_cond.mutex());
    status = m_status;
    //Check if camera is not waiting for trigger
    if(status == Camera::Exposure &&
       m_trigger_mode == IntTrigMult)
      {
	// Check the frame start trigger acquisition status
	// Set the acquisition status selector
	camera.AcquisitionStatusSelector.SetValue
	  (AcquisitionStatusSelector_FrameTriggerWait);
	// Read the acquisition status
	bool IsWaitingForFrameTrigger = camera.AcquisitionStatus.GetValue();
	status = IsWaitingForFrameTrigger ? Camera::Ready : status;
	DEB_TRACE() << DEB_VAR1(IsWaitingForFrameTrigger);
      }
    DEB_RETURN() << DEB_VAR1(status);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::_setStatus(Camera::Status status,bool force)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    if(force || m_status != Camera::Fault)
        m_status = status;
    m_cond.broadcast();
}
//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getFrameRate(double& frame_rate)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        frame_rate = static_cast<double>(camera.ResultingFrameRateAbs.GetValue());
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }        
    DEB_RETURN() << DEB_VAR1(frame_rate);
}
//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setTimeout(int TO)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(TO);    
    m_timeout = TO;
}
//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::checkRoi(const Roi& set_roi, Roi& hw_roi)
{
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    _checkRoi(camera, set_roi, hw_roi);
}

void Camera::_checkRoi(Camera_t& camera, const Roi& set_roi, Roi& hw_roi)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(set_roi);
    try
    {
        if (set_roi.isActive())
        {
            const Size& aSetRoiSize = set_roi.getSize();
            Size aRoiSize = Size(max(aSetRoiSize.getWidth(),
				     int(camera.Width.GetMin())),
                                 max(aSetRoiSize.getHeight(),
				     int(camera.Height.GetMin())));
            hw_roi = Roi(set_roi.getTopLeft(), aRoiSize);
        }
        else
            hw_roi = set_roi;
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
    DEB_RETURN() << DEB_VAR1(hw_roi);

}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setRoi(const Roi& ask_roi)
{
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    _setRoi(camera, ask_roi);
}

void Camera::_setRoi(Camera_t& camera, const Roi& ask_roi)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(ask_roi);
    Roi set_roi;
    _checkRoi(camera, ask_roi,set_roi);
    Roi r;    
    try
    {
        //- backup old roi, in order to rollback if error
        _getRoi(camera, r);
        if(r == set_roi) return;
        
        //- first reset the ROI
        camera.OffsetX.SetValue(camera.OffsetX.GetMin());
        camera.OffsetY.SetValue(camera.OffsetY.GetMin());
        camera.Width.SetValue(camera.Width.GetMax());
        camera.Height.SetValue(camera.Height.GetMax());
        
        Roi fullFrame(  camera.OffsetX.GetMin(),
						camera.OffsetY.GetMin(),
						camera.Width.GetMax(),
						camera.Height.GetMax());
        
        if(set_roi.isActive() && fullFrame != set_roi)
        {
            //- then fix the new ROI
            camera.Width.SetValue( set_roi.getSize().getWidth());
            camera.Height.SetValue(set_roi.getSize().getHeight());
            camera.OffsetX.SetValue(set_roi.getTopLeft().x);
            camera.OffsetY.SetValue(set_roi.getTopLeft().y);
        }
    }
    catch (GenICam::GenericException &e)
    {
        try
        {
            //-  rollback the old roi
            camera.Width.SetValue( r.getSize().getWidth());
            camera.Height.SetValue(r.getSize().getHeight());
            camera.OffsetX.SetValue(r.getTopLeft().x);
            camera.OffsetY.SetValue(r.getTopLeft().y);
            // Error handling
        }
        catch (GenICam::GenericException &e2)
        {
            THROW_HW_ERROR(Error) << e2.GetDescription();
        }
        THROW_HW_ERROR(Error) << e.GetDescription();
    }        

}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getRoi(Roi& hw_roi)
{
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    _getRoi(camera, hw_roi);
}

void Camera::_getRoi(Camera_t& camera, Roi& hw_roi)
{
    DEB_MEMBER_FUNCT();
    try
    {
        Roi  r( static_cast<int>(camera.OffsetX()),
                static_cast<int>(camera.OffsetY()),
                static_cast<int>(camera.Width())    ,
                static_cast<int>(camera.Height())
        );
        
        hw_roi = r;
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }    
    DEB_RETURN() << DEB_VAR1(hw_roi);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::checkBin(Bin &aBin)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        int x = aBin.getX();
        if (x > camera.BinningHorizontal.GetMax())
            x = camera.BinningHorizontal.GetMax();

        int y = aBin.getY();
        if (y > camera.BinningVertical.GetMax())
            y = camera.BinningVertical.GetMax();
        aBin = Bin(x, y);
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
    DEB_RETURN() << DEB_VAR1(aBin);
}
//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setBin(const Bin &aBin)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    _freeStreamGrabber();
    try
    {
        camera.BinningVertical.SetValue(aBin.getY());
        camera.BinningHorizontal.SetValue(aBin.getX());
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
    DEB_RETURN() << DEB_VAR1(aBin);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getBin(Bin &aBin)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
    	aBin = Bin(camera.BinningVertical.GetValue(), camera.BinningHorizontal.GetValue());
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
    DEB_RETURN() << DEB_VAR1(aBin);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
bool Camera::isBinningAvailable()
{
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    return _isBinningAvailable(camera);
}

bool Camera::_isBinningAvailable(Camera_t& camera)
{
    DEB_MEMBER_FUNCT();
    bool isAvailable = false;
    try
    {
      isAvailable = (GenApi::IsAvailable(camera.BinningVertical) &&
		     GenApi::IsAvailable(camera.BinningHorizontal));
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
    DEB_RETURN() << DEB_VAR1(isAvailable);
    return isAvailable;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
bool Camera::isRoiAvailable()
{
  Basler basler_cam = this->_getBasler();
  Camera_t& camera = basler_cam.camera;
  return _isRoiAvailable(camera);
}

bool Camera::_isRoiAvailable(Camera_t& camera)
{
  DEB_MEMBER_FUNCT();
  bool isAvailable = false;
  try
    {
      isAvailable = (GenApi::IsAvailable(camera.OffsetX) && GenApi::IsWritable(camera.OffsetX) &&
		     GenApi::IsAvailable(camera.OffsetY) && GenApi::IsWritable(camera.OffsetY) &&
		     GenApi::IsAvailable(camera.Width) && GenApi::IsWritable(camera.Width) &&
		     GenApi::IsAvailable(camera.Height) && GenApi::IsWritable(camera.Height));
    }
  catch(GenICam::GenericException &e)
    {
      DEB_WARNING() << e.GetDescription();
    }
  DEB_RETURN() << DEB_VAR1(isAvailable);
  return isAvailable;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setPacketSize(int isize)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(isize);
    DEB_TRACE()<<"setPacketSize : "<<isize;
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        camera.GevSCPSPacketSize.SetValue(isize);
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getPacketSize(int& isize)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        isize = camera.GevSCPSPacketSize.GetValue();
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setInterPacketDelay(int ipd)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(ipd);
    DEB_TRACE()<<"setInterPacketDelay : "<<ipd;
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        camera.GevSCPD.SetValue(ipd);
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getInterPacketDelay(int& ipd)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        ipd = camera.GevSCPD.GetValue();
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setSocketBufferSize(int sbs)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(sbs);
    m_socketBufferSize = sbs;
}

//-----------------------------------------------------
// isGainAvailable
//-----------------------------------------------------
bool Camera::isGainAvailable()
{
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    return GenApi::IsAvailable(camera.GainRaw);
}


//-----------------------------------------------------
// isAutoGainAvailable
//-----------------------------------------------------
bool Camera::isAutoGainAvailable()
{
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    return GenApi::IsAvailable(camera.GainAuto);
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getBandwidthAssigned(int& ipd)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        ipd = camera.GevSCBWA.GetValue();
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}   
//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getMaxThroughput(int& ipd)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        ipd = camera.GevSCDMT.GetValue();
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}    

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getCurrentThroughput(int& ipd)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        ipd = camera.GevSCDCT.GetValue();
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}    
    
//-----------------------------------------------------
//
//-----------------------------------------------------

void Camera::getTemperature(double& temperature)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        // If the parameter TemperatureAbs is available for this camera
        if (GenApi::IsAvailable(camera.TemperatureAbs))
            temperature = camera.TemperatureAbs.GetValue();
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setAutoGain(bool auto_gain)
{
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;

}

void Camera::_setAutoGain(Camera_t& camera, bool auto_gain)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(auto_gain);
    try
    {
        if (GenApi::IsAvailable(camera.GainAuto) && GenApi::IsAvailable(camera.GainSelector))
        {
            if (!auto_gain)
            {
                camera.GainAuto.SetValue(GainAuto_Off);
                camera.GainSelector.SetValue(GainSelector_All);
            }
            else
            {
                camera.GainAuto.SetValue(GainAuto_Continuous);
            }
        }
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getAutoGain(bool& auto_gain)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        if (GenApi::IsAvailable(camera.GainAuto))
        {
            auto_gain = camera.GainAuto.GetValue();
        }
        else
        {
            auto_gain = false;
//			THROW_HW_ERROR(Error)<<"GainAuto Parameter is not Available !";			
        }
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }

    DEB_RETURN() << DEB_VAR1(auto_gain);
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setGain(double gain)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(gain);
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        // you want to set the gain, remove autogain
        if (GenApi::IsAvailable(camera.GainAuto))
        {		
	  _setAutoGain(camera, false);
	}
		
        if (GenApi::IsWritable(camera.GainRaw) && GenApi::IsAvailable(camera.GainRaw))
        {

            int low_limit = camera.GainRaw.GetMin();
            DEB_TRACE() << "low_limit = " << low_limit;

            int hight_limit = camera.GainRaw.GetMax();
            DEB_TRACE() << "hight_limit = " << hight_limit;

            int gain_raw = int((hight_limit - low_limit) * gain + low_limit);

            if (gain_raw < low_limit)
            {
                gain_raw = low_limit;
            }
            else if (gain_raw > hight_limit)
            {
                gain_raw = hight_limit;
            }
            camera.GainRaw.SetValue(gain_raw);
            DEB_TRACE() << "gain_raw = " << gain_raw;
        }
		else
		{
			THROW_HW_ERROR(Error)<<"GainRaw Parameter is not Available !";
		}
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();		
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getGain(double& gain)
{
    DEB_MEMBER_FUNCT();
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        if (GenApi::IsAvailable(camera.GainRaw))
        {
            int gain_raw = camera.GainRaw.GetValue();
            int low_limit = camera.GainRaw.GetMin();
            int hight_limit = camera.GainRaw.GetMax();

            gain = double(gain_raw - low_limit) / (hight_limit - low_limit);
        }
        else
        {
            gain = 0.;
        }
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
    DEB_RETURN() << DEB_VAR1(gain);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setFrameTransmissionDelay(int ftd)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(ftd);
    Basler basler_cam = this->_getBasler();
    Camera_t& camera = basler_cam.camera;
    try
    {
        camera.GevSCFTD.SetValue(ftd);
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
}

//---------------------------
//- Camera::reset()
//---------------------------
void Camera::reset()
{
    DEB_MEMBER_FUNCT();
    try
    {
        _stopAcq(false);
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }    
}

void Camera::isColor(bool& color_flag) const
{
  color_flag = m_color_flag;
}

void Camera::hasVideoCapability(bool& has_video_capability) const
{
  has_video_capability = m_video_flag_mode;
}

void Camera::setOutput1LineSource(Camera::LineSource source)
{
  DEB_MEMBER_FUNCT();
  DEB_PARAM() << DEB_VAR1(source);
  Basler basler_cam = this->_getBasler();
  Camera_t& camera = basler_cam.camera;

  camera.LineSelector.SetValue(LineSelector_Out1);

  LineSourceEnums line_src;
  switch(source)
    {
    case Camera::Off:				line_src = LineSource_Off;			break;
    case Camera::ExposureActive:		line_src = LineSource_ExposureActive;		break;
    case Camera::FrameTriggerWait:		line_src = LineSource_FrameTriggerWait;		break;
    case Camera::LineTriggerWait:		line_src = LineSource_LineTriggerWait;		break;
    case Camera::Timer1Active:			line_src = LineSource_Timer1Active;		break;
    case Camera::Timer2Active:			line_src = LineSource_Timer2Active;		break;
    case Camera::Timer3Active:			line_src = LineSource_Timer3Active;		break;
    case Camera::Timer4Active:			line_src = LineSource_Timer4Active;		break;
    case Camera::TimerActive:			line_src = LineSource_TimerActive;		break;
    case Camera::UserOutput1:			line_src = LineSource_UserOutput1;		break;
    case Camera::UserOutput2:			line_src = LineSource_UserOutput2;		break;
    case Camera::UserOutput3:			line_src = LineSource_UserOutput3;		break;
    case Camera::UserOutput4:			line_src = LineSource_UserOutput4;		break;
    case Camera::UserOutput:			line_src = LineSource_UserOutput;		break;
    case Camera::TriggerReady:			line_src = LineSource_TriggerReady;		break;
    case Camera::SerialTx:			line_src = LineSource_SerialTx;			break;
    case Camera::AcquisitionTriggerWait:	line_src = LineSource_AcquisitionTriggerWait;	break;
    case Camera::ShaftEncoderModuleOut:		line_src = LineSource_ShaftEncoderModuleOut;	break;
    case Camera::FrequencyConverter:		line_src = LineSource_FrequencyConverter;	break;
    case Camera::PatternGenerator1:		line_src = LineSource_PatternGenerator1;	break;
    case Camera::PatternGenerator2:		line_src = LineSource_PatternGenerator2;	break;
    case Camera::PatternGenerator3:		line_src = LineSource_PatternGenerator3;	break;
    case Camera::PatternGenerator4:		line_src = LineSource_PatternGenerator4;	break;
    case Camera::AcquisitionTriggerReady:	line_src = LineSource_AcquisitionTriggerReady;	break;
    default:
      THROW_HW_ERROR(NotSupported) << "Not yet managed";
    }
  camera.LineSource.SetValue(line_src);
}

void Camera::getOutput1LineSource(Camera::LineSource& source)
{
  DEB_MEMBER_FUNCT();
  Basler basler_cam = this->_getBasler();
  Camera_t& camera = basler_cam.camera;

  camera.LineSelector.SetValue(LineSelector_Out1);
  switch(camera.LineSource.GetValue())
    {
    case LineSource_Off:			source = Off;				break;
    case LineSource_ExposureActive:		source = ExposureActive;		break;
    case LineSource_FrameTriggerWait:		source = FrameTriggerWait;		break;
    case LineSource_LineTriggerWait:		source = LineTriggerWait;		break;
    case LineSource_Timer1Active:		source = Timer1Active;			break;
    case LineSource_Timer2Active:		source = Timer2Active;			break;
    case LineSource_Timer3Active:		source = Timer3Active;			break;
    case LineSource_Timer4Active:		source = Timer4Active;			break;
    case LineSource_TimerActive:		source = TimerActive;			break;
    case LineSource_UserOutput1:		source = UserOutput1;			break;
    case LineSource_UserOutput2:		source = UserOutput2;			break;
    case LineSource_UserOutput3:		source = UserOutput3;			break;
    case LineSource_UserOutput4:		source = UserOutput4;			break;
    case LineSource_UserOutput:			source = UserOutput;			break;
    case LineSource_TriggerReady:		source = TriggerReady;			break;
    case LineSource_SerialTx:			source = SerialTx;			break;
    case LineSource_AcquisitionTriggerWait:	source = AcquisitionTriggerWait;	break;
    case LineSource_ShaftEncoderModuleOut:	source = ShaftEncoderModuleOut;		break;
    case LineSource_FrequencyConverter:		source = FrequencyConverter;		break;
    case LineSource_PatternGenerator1:		source = PatternGenerator1;		break;
    case LineSource_PatternGenerator2:		source = PatternGenerator2;		break;
    case LineSource_PatternGenerator3:		source = PatternGenerator3;		break;
    case LineSource_PatternGenerator4:		source = PatternGenerator4;		break;
    case LineSource_AcquisitionTriggerReady:	source = AcquisitionTriggerReady;	break;
    default:
      THROW_HW_ERROR(Error) << "Don't know this value ;)";
    }

  DEB_RETURN() << DEB_VAR1(source);
}
//---------------------------
// The Total Buffer Count will count the number of all buffers with "status == succeeded" and "status == failed". 
// That means, all successfully and all incompletely grabbed (error code: 0xE1000014) buffers. 
// That means, the Failed Buffer Count will also be included into this number.
//---------------------------
void Camera::getStatisticsTotalBufferCount(long& count)
{
	DEB_MEMBER_FUNCT();
	if(StreamGrabber_ != NULL)
		count = StreamGrabber_->Statistic_Total_Buffer_Count.GetValue();
	else
		count = -1;//Because Not valid when acquisition is stopped
}

//---------------------------    
// The Failed Buffer Count will count only buffers, which were received with "status == failed". 
// That means, buffers that were incompletely grabbed (error code: 0xE1000014).
//---------------------------
void Camera::getStatisticsFailedBufferCount(long& count)
{
	DEB_MEMBER_FUNCT();
	if(StreamGrabber_ != NULL)
		count = StreamGrabber_->Statistic_Failed_Buffer_Count.GetValue();
	else
		count = -1;//Because Not valid when acquisition is stopped
}
//---------------------------    

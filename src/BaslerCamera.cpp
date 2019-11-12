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
          Camera_(NULL),
          StreamGrabber_(NULL),
          m_receive_priority(receive_priority),
	  m_video_flag_mode(false),
	  m_video(NULL),
	  m_buffer_size(0)
{
    DEB_CONSTRUCTOR();
    m_camera_id = camera_id;
    try
    {
        // Create the transport layer object needed to enumerate or
        // create a camera object of type Camera_t::DeviceClass()
        DEB_TRACE() << "Create a camera object of type Camera_t::DeviceClass()";
        CTlFactory& TlFactory = CTlFactory::GetInstance();

        CBaslerGigEDeviceInfo di;

	// by default use ip:// scheme if none is given
	if (m_camera_id.find("://") == std::string::npos)
	{
            m_camera_id = "ip://" + m_camera_id;
	}

	if(!m_camera_id.compare(0, IP_PREFIX.size(), IP_PREFIX))
        {
            // m_camera_id is not really necessarily an IP, it may also be a DNS name
            Pylon::String_t pylon_camera_ip(_get_ip_addresse(m_camera_id.substr(IP_PREFIX.size()).c_str()));
            //- Find the Pylon device thanks to its IP Address
            di.SetIpAddress( pylon_camera_ip);
            DEB_TRACE() << "Create the Pylon device attached to ip address: "
			<< DEB_VAR1(m_camera_id);
 	}
        else if (!m_camera_id.compare(0, SN_PREFIX.size(), SN_PREFIX))
	{
            Pylon::String_t serial_number(m_camera_id.substr(SN_PREFIX.size()).c_str());
            //- Find the Pylon device thanks to its serial number
            di.SetSerialNumber(serial_number);
            DEB_TRACE() << "Create the Pylon device attached to serial number: "
			<< DEB_VAR1(m_camera_id);
	}
	else if(!m_camera_id.compare(0, UNAME_PREFIX.size(), UNAME_PREFIX))
        {
            Pylon::String_t user_name(m_camera_id.substr(UNAME_PREFIX.size()).c_str());
            //- Find the Pylon device thanks to its user name
            di.SetUserDefinedName(user_name);
            DEB_TRACE() << "Create the Pylon device attached to user name: "
			<< DEB_VAR1(m_camera_id);
 	}
	else 
        {
	    THROW_CTL_ERROR(InvalidValue) << "Unrecognized camera id: " << camera_id;
        }

        IPylonDevice* device = TlFactory.CreateDevice( di);
        if (!device)
        {
            THROW_HW_ERROR(Error) << "Unable to find camera with selected IP!";
        }

        //- Create the Basler Camera object
        DEB_TRACE() << "Create the Camera object corresponding to the created Pylon device";
        Camera_ = new Camera_t(device);
        if(!Camera_)
        {
            THROW_HW_ERROR(Error) << "Unable to get the camera from transport_layer!";
        }

        //- Get detector model and type
        m_detector_type  = Camera_->GetDeviceInfo().GetVendorName();
        m_detector_model = Camera_->GetDeviceInfo().GetModelName();

        //- Infos:
        DEB_TRACE() << DEB_VAR2(m_detector_type,m_detector_model);
        DEB_TRACE() << "SerialNumber    = " << Camera_->GetDeviceInfo().GetSerialNumber();
        DEB_TRACE() << "UserDefinedName = " << Camera_->GetDeviceInfo().GetUserDefinedName();
        DEB_TRACE() << "DeviceVersion   = " << Camera_->GetDeviceInfo().GetDeviceVersion();
        DEB_TRACE() << "DeviceFactory   = " << Camera_->GetDeviceInfo().GetDeviceFactory();
        DEB_TRACE() << "FriendlyName    = " << Camera_->GetDeviceInfo().GetFriendlyName();
        DEB_TRACE() << "FullName        = " << Camera_->GetDeviceInfo().GetFullName();
        DEB_TRACE() << "DeviceClass     = " << Camera_->GetDeviceInfo().GetDeviceClass();

        // Open the camera
        DEB_TRACE() << "Open camera";        
        Camera_->Open();
    
        if(packet_size > 0)
          Camera_->GevSCPSPacketSize.SetValue(packet_size);
    
        // Set the image format and AOI
        DEB_TRACE() << "Set the image format and AOI";
	// basler model string last character codes for color (c) or monochrome (m)
	std::list<string> formatList;

	if (m_detector_model.find("gc") != std::string::npos)
	  {
	    // The list Order here has sense, if supported, the first format in the list will be applied
	    // as default one, and in case of color camera the default will defined the max buffer
	    // size for the memory allocation. Since YUV422Packed is 1.5 byte per pixel it is available
	    // before the Bayer 8bit (1 byte per pixel).

	    formatList.push_back(string("BayerRG16"));
	    formatList.push_back(string("BayerBG16"));
	    formatList.push_back(string("BayerRG12"));
	    formatList.push_back(string("BayerBG12"));
	    formatList.push_back(string("YUV422Packed"));
	    formatList.push_back(string("BayerRG8"));
	    formatList.push_back(string("BayerBG8"));
	    m_color_flag = true;
	  }
	else
	  {
	    formatList.push_back(string("Mono16"));
	    formatList.push_back(string("Mono12"));
	    formatList.push_back(string("Mono8"));
	    m_color_flag = false;
	  }

        bool formatSetFlag = false;
	for(list<string>::iterator it = formatList.begin(); it != formatList.end(); it++)
        {
	  GenApi::IEnumEntry *anEntry = Camera_->PixelFormat.GetEntryByName((*it).c_str());
            if(anEntry && GenApi::IsAvailable(anEntry))
            {
                formatSetFlag = true;
		Camera_->PixelFormat.SetIntValue(anEntry->GetValue());
                DEB_TRACE() << "Set pixel format to " << *it;
                break;
            }
        }
        if(!formatSetFlag)
            THROW_HW_ERROR(Error) << "Unable to set PixelFormat for the camera!";
        
        DEB_TRACE() << "Set the ROI to full frame";
	if(isRoiAvailable())
	  {
	    Roi aFullFrame(0,0,Camera_->WidthMax(),Camera_->HeightMax());
	    setRoi(aFullFrame);
	  }
        // Set Binning to 1, only if the camera has this functionality        
        if (isBinningAvailable())
        {
            DEB_TRACE() << "Set BinningH & BinningV to 1";
            Camera_->BinningVertical.SetValue(1);
            Camera_->BinningHorizontal.SetValue(1);
        }

        DEB_TRACE() << "Get the Detector Max Size";
        m_detector_size = Size(Camera_->WidthMax(), Camera_->HeightMax());

        // Set the camera to continuous frame mode
        DEB_TRACE() << "Set the camera to continuous frame mode";
        Camera_->TriggerSelector.SetValue(TriggerSelector_AcquisitionStart);
        Camera_->AcquisitionMode.SetValue(AcquisitionMode_Continuous);
        
        if ( GenApi::IsAvailable(Camera_->ExposureAuto ))
        {
            DEB_TRACE() << "Set ExposureAuto to Off";           
            Camera_->ExposureAuto.SetValue(ExposureAuto_Off);
        }

	if (GenApi::IsAvailable(Camera_->TestImageSelector ))
	{
            DEB_TRACE() << "Set TestImage to Off";           
            Camera_->TestImageSelector.SetValue(TestImageSelector_Off);	  
	}
	// Start with internal trigger
	// Force cache variable to get trigger really initialized at first call
	m_trigger_mode = ExtTrigSingle;
	setTrigMode(IntTrig);

        WaitObject_ = WaitObjectEx::Create();
    
        m_acq_thread = new _AcqThread(*this);
        m_acq_thread->start();
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }

    // if color camera video capability will be available
    m_video_flag_mode = m_color_flag;

    // initialize temp. buffers to null pointer
    for(int i = 0;i < NB_TMP_BUFFER;++i) m_tmp_buffer[i] = NULL;
}

//---------------------------
//- Dtor
//---------------------------
Camera::~Camera()
{
    DEB_DESTRUCTOR();
    try
    {
        // Stop Acq thread
        delete m_acq_thread;
        m_acq_thread = NULL;
        
        // Close stream grabber
        DEB_TRACE() << "Close stream grabber";
	_freeStreamGrabber();

        // Close camera
        DEB_TRACE() << "Close camera";
        delete Camera_;
        Camera_ = NULL;
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

    try
    {
      _freeStreamGrabber();
      // For video (color camera or B/W forced to video) use a small 2-frames Tmp buffer
      // to not stop acq if some frames are missing but just return the last acquired
      // for other modes the SoftBuffer is filled by Pylon Grabber, and an frame error will
      // stop the acquisition
      if(m_video_flag_mode || m_nb_frames == 0)
	_initStreamGrabber(TmpBuffer);
      else
	_initStreamGrabber(SoftBuffer);
      

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
	    this->Camera_->TriggerSoftware.Execute();
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

  Camera_->AcquisitionStart.Execute();

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
            Camera_->AcquisitionStop.Execute();
	    
	    // always free for both video or acquisition
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

      // Free temporary buffer used when live mode or camera is color
      for(int i = 0;i < NB_TMP_BUFFER && m_tmp_buffer[i] != NULL; ++i)
	{
#ifdef __unix
	  free(m_tmp_buffer[i]);
#else
	  _aligned_free(m_tmp_buffer[i]);
#endif
	  m_tmp_buffer[i]=NULL;
	}
    }
}

void Camera::_forceVideoMode(bool force)
{
  DEB_MEMBER_FUNCT();
  m_video_flag_mode = force;
  
}
void Camera::_allocTmpBuffer()
{
  DEB_MEMBER_FUNCT();
  for(int i = 0;i < NB_TMP_BUFFER;++i)
    {
#ifdef __unix
      int ret=posix_memalign(&m_tmp_buffer[i],16,m_buffer_size);
      if (ret)
	THROW_HW_ERROR(Error) << "posix_memalign(): request for aligned memory allocation failed";
#else
      m_tmp_buffer[i] = _aligned_malloc(m_buffer_size,16);
      if (m_tmp_buffer[i] == NULL)
	THROW_HW_ERROR(Error) << "_aligned_malloc(): request for aligned memory allocation failed";	
#endif
    }
}

void Camera::_initStreamGrabber(BufferMode mode)
{
  DEB_MEMBER_FUNCT();

  // Get the first stream grabber object of the selected camera
  StreamGrabber_ = new Camera_t::StreamGrabber_t(Camera_->GetStreamGrabber(0));
  DEB_TRACE() << "Get the first stream grabber object of the selected camera";

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
  // We won't use image buffers greater than PayLoadSize, the real frame size
  size_t payload_size = (size_t)Camera_->PayloadSize.GetValue();  
  StreamGrabber_->MaxBufferSize.SetValue((const size_t)payload_size);

  if (m_video_flag_mode)
    {
      m_buffer_size = payload_size;
    }
  else
    {
      // Store the SoftBuffer buffer size
      m_buffer_size =m_buffer_ctrl_obj.getBuffer().getFrameDim().getMemSize();
      if (payload_size < m_buffer_size)
	THROW_HW_ERROR(Error) << " buffer frame size bigger than payload size";
    }
  
  // For color camera or live acquisition, use temporary buffer
  // for b/w camera or classic acquisition use directly the soft buffer manager
  if (mode == TmpBuffer)
    {
      // Alloc first the temporary buffer
      _allocTmpBuffer();
      DEB_TRACE() << "We'll queue " << NB_TMP_BUFFER << " image buffers";
      StreamGrabber_->MaxNumBuffer.SetValue(NB_TMP_BUFFER);
      DEB_TRACE() << "Allocate all resources for grabbing, PrepareGrab";
      StreamGrabber_->PrepareGrab();
      
      for(int i = 0;i < NB_TMP_BUFFER;++i)
	{
	  StreamBufferHandle bufferId = StreamGrabber_->RegisterBuffer(m_tmp_buffer[i],
								       (const size_t)m_buffer_size);
	  StreamGrabber_->QueueBuffer(bufferId,NULL);
	}
    }
  else // SoftBuffer
    {
      StdBufferCbMgr& buffer_mgr = m_buffer_ctrl_obj.getBuffer();
      int nb_buffers;
      buffer_mgr.getNbBuffers(nb_buffers);      
      DEB_TRACE() << "We'll queue " << nb_buffers << " image buffers";
      StreamGrabber_->MaxNumBuffer.SetValue(nb_buffers);
      DEB_TRACE() << "Allocate all resources for grabbing, PrepareGrab";
      StreamGrabber_->PrepareGrab();
      
      for(int i = 0;i < nb_buffers;++i)
	{
	  void *ptr = buffer_mgr.getFrameBufferPtr(i);	  
	  StreamBufferHandle bufferId = StreamGrabber_->RegisterBuffer(ptr,
								       (const size_t)m_buffer_size);
	StreamGrabber_->QueueBuffer(bufferId,NULL);
      }
    }
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
				    if (m_cam.m_nb_frames == 0 || 
					m_cam.m_image_number < int(m_cam.m_nb_frames - nb_buffers))
				      m_cam.StreamGrabber_->QueueBuffer(Result.Handle(),NULL);
                                
				    HwFrameInfoType frame_info;
				    frame_info.acq_frame_nb = m_cam.m_image_number;
				    // copy TmpBuffer frame to SoftBuffer frame room
				    if (m_cam.m_nb_frames == 0)
				      {
					void *ptr = buffer_mgr.getFrameBufferPtr(m_cam.m_image_number);
					memcpy(ptr, (void *)Result.Buffer(), m_cam.m_buffer_size);
				      }
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
				      case PixelType_YUV411packed:  	mode = YUV411PACKED;	break;
				      case PixelType_YUV422packed:  	mode = YUV422PACKED;	break;
				      case PixelType_YUV444packed:  	mode = YUV444PACKED;	break;
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

    // get the max image size of the detector (the chip)
    size = m_detector_size;
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getImageType(ImageType& type)
{
    DEB_MEMBER_FUNCT();
    PixelFormatEnums ps;
    try
    {
        ps = Camera_->PixelFormat.GetValue();
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
    switch( ps )
      {
      case PixelFormat_Mono8:
      case PixelFormat_BayerRG8:
      case PixelFormat_BayerBG8:
      case PixelFormat_RGB8Packed:
      case PixelFormat_BGR8Packed:
      case PixelFormat_RGBA8Packed:
      case PixelFormat_BGRA8Packed:
      case PixelFormat_YUV411Packed:
      case PixelFormat_YUV422Packed:
      case PixelFormat_YUV444Packed:
	type= Bpp8;
	break;

      case PixelFormat_Mono10:
      case PixelFormat_BayerRG10:
      case PixelFormat_BayerBG10:
	type = Bpp10;
	break;
	
      case PixelFormat_Mono12:
      case PixelFormat_BayerRG12:
      case PixelFormat_BayerBG12:
	type= Bpp12;
	break;
        
      case PixelFormat_Mono16: //- this is in fact 12 bpp inside a 16bpp image
      case PixelFormat_BayerRG16:
      case PixelFormat_BayerBG16:
	type= Bpp16;
	break;
	    
      default:
	THROW_HW_ERROR(Error) << "Unsupported Pixel Format : " << ps;
	break;
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
        switch( type )
        {
            case Bpp8:
                this->Camera_->PixelFormat.SetValue(PixelFormat_Mono8);
                break;
            case Bpp12:
                this->Camera_->PixelFormat.SetValue(PixelFormat_Mono12);
                break;
            case Bpp16:
                this->Camera_->PixelFormat.SetValue(PixelFormat_Mono16);
                break;

            default:
                THROW_HW_ERROR(NotSupported) << "Cannot change the pixel format of the camera !";
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
    type = m_detector_type;
    return;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getDetectorModel(string& type)
{
    DEB_MEMBER_FUNCT();
    type = m_detector_model;
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
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(mode);

    if(mode == m_trigger_mode)
      return;			// Nothing to do
    
    try
    {        
        GenApi::IEnumEntry *enumEntryFrameStart = Camera_->TriggerSelector.GetEntryByName("FrameStart");
        if(enumEntryFrameStart && GenApi::IsAvailable(enumEntryFrameStart))
            this->Camera_->TriggerSelector.SetValue( TriggerSelector_FrameStart );
        else
            this->Camera_->TriggerSelector.SetValue( TriggerSelector_AcquisitionStart );

    	if ( mode == IntTrig )
        {
            //- INTERNAL
            this->Camera_->TriggerMode.SetValue( TriggerMode_Off );
            this->Camera_->ExposureMode.SetValue(ExposureMode_Timed);
	    this->Camera_->AcquisitionFrameRateEnable.SetValue(true);
	}
        else if ( mode == IntTrigMult )
        {
	    this->Camera_->TriggerMode.SetValue(TriggerMode_On);
	    this->Camera_->TriggerSource.SetValue(TriggerSource_Software);
	    if (GenApi::IsAvailable(Camera_->AcquisitionFrameCount))
	      this->Camera_->AcquisitionFrameCount.SetValue(1);
	    this->Camera_->ExposureMode.SetValue(ExposureMode_Timed);
        }
        else if ( mode == ExtGate )
        {
            //- EXTERNAL - TRIGGER WIDTH
            this->Camera_->TriggerMode.SetValue( TriggerMode_On );
	    this->Camera_->TriggerSource.SetValue(TriggerSource_Line1);
            this->Camera_->AcquisitionFrameRateEnable.SetValue( false );
            this->Camera_->ExposureMode.SetValue( ExposureMode_TriggerWidth );
        }        
        else //ExtTrigMult
        {
            this->Camera_->TriggerMode.SetValue( TriggerMode_On );
	    this->Camera_->TriggerSource.SetValue(TriggerSource_Line1);
            this->Camera_->AcquisitionFrameRateEnable.SetValue( false );
            this->Camera_->ExposureMode.SetValue( ExposureMode_Timed );
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
    try
    {
        acqStart =  this->Camera_->TriggerMode.GetValue();

        GenApi::IEnumEntry *enumEntryFrameStart = Camera_->TriggerSelector.GetEntryByName("FrameStart");  
        if(enumEntryFrameStart && GenApi::IsAvailable(enumEntryFrameStart))            
        {
            this->Camera_->TriggerSelector.SetValue( TriggerSelector_FrameStart );
            frameStart =  this->Camera_->TriggerMode.GetValue();
        }

        expMode = this->Camera_->ExposureMode.GetValue();
    
	if(acqStart == TriggerMode_On)
	  {
	    int source = this->Camera_->TriggerSource.GetValue();
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
void Camera::setTrigActivation(TrigActivation activation)
{
    DEB_MEMBER_FUNCT();
    try
    {
        Basler_GigECamera::TriggerActivationEnums act =
            static_cast<Basler_GigECamera::TriggerActivationEnums>(activation);

        // If the parameter TriggerActivation is available for this camera
        if (GenApi::IsAvailable(Camera_->TriggerActivation))
            Camera_->TriggerActivation.SetValue(act);
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getTrigActivation(TrigActivation& activation) const
{
    DEB_MEMBER_FUNCT();
    try
    {
        Basler_GigECamera::TriggerActivationEnums act;

        // If the parameter AcquisitionFrameCount is available for this camera
        if (GenApi::IsAvailable(Camera_->TriggerActivation))
            act = Camera_->TriggerActivation.GetValue();

        activation = static_cast<TrigActivation>(act);

    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setExpTime(double exp_time)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(exp_time);
    
    
    TrigMode mode;
    getTrigMode(mode);
    
    try
    {
        if(mode !=  ExtGate) { // the expTime can not be set in ExtGate!
            if (GenApi::IsAvailable(Camera_->ExposureTimeBaseAbs))
            {
                //If scout or pilot, exposure time has to be adjusted using
                // the exposure time base + the exposure time raw.
                //see ImageGrabber for more details !!!
                Camera_->ExposureTimeBaseAbs.SetValue(100.0); //- to be sure we can set the Raw setting on the full range (1 .. 4095)
                double raw = ::ceil(exp_time / 50);
                Camera_->ExposureTimeRaw.SetValue(static_cast<int> (raw));
                raw = static_cast<double> (Camera_->ExposureTimeRaw.GetValue());
                Camera_->ExposureTimeBaseAbs.SetValue(1E6 * (exp_time / raw));
		DEB_TRACE() << "raw = " << raw;
		DEB_TRACE() << "ExposureTimeBaseAbs = " << (1E6 * (exp_time / raw));			
            }
            else
            {
                // More recent model like ACE and AVIATOR support direct programming of the exposure using
                // the exposure time absolute.
                Camera_->ExposureTimeAbs.SetValue(1E6 * exp_time);
            }
        }
        
        m_exp_time = exp_time;

        // set the frame rate using expo time + latency
        if (m_latency_time < 1e-6) // Max camera speed
        {
            Camera_->AcquisitionFrameRateEnable.SetValue(false);
        }
        else
        {
            double periode = m_latency_time + m_exp_time;
            Camera_->AcquisitionFrameRateEnable.SetValue(true);
            Camera_->AcquisitionFrameRateAbs.SetValue(1 / periode);
            DEB_TRACE() << DEB_VAR1(Camera_->AcquisitionFrameRateAbs.GetValue());
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
    try
    {
        double value = 1.0E-6 * static_cast<double>(Camera_->ExposureTimeAbs.GetValue());    
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
void Camera::getExposureTimeRange(double& min_expo, double& max_expo) const
{
    DEB_MEMBER_FUNCT();

    try
    {
        // Pilot and and Scout do not have TimeAbs capability
        if (GenApi::IsAvailable(Camera_->ExposureTimeBaseAbs))
        {
            // memorize initial value of exposure time
            DEB_TRACE() << "memorize initial value of exposure time";
            int initial_raw = Camera_->ExposureTimeRaw.GetValue();
            DEB_TRACE() << "initial_raw = " << initial_raw;
            double initial_base = Camera_->ExposureTimeBaseAbs.GetValue();
            DEB_TRACE() << "initial_base = " << initial_base;

            DEB_TRACE() << "compute Min/Max allowed values of exposure time";
            // fix raw/base in order to get the Max of Exposure            
            Camera_->ExposureTimeBaseAbs.SetValue(Camera_->ExposureTimeBaseAbs.GetMax());
            max_expo = 1E-06 * Camera_->ExposureTimeBaseAbs.GetValue() * Camera_->ExposureTimeRaw.GetMax();
            DEB_TRACE() << "max_expo = " << max_expo << " (s)";

            // fix raw/base in order to get the Min of Exposure            
            Camera_->ExposureTimeBaseAbs.SetValue(Camera_->ExposureTimeBaseAbs.GetMin());
            min_expo = 1E-06 * Camera_->ExposureTimeBaseAbs.GetValue() * Camera_->ExposureTimeRaw.GetMin();
            DEB_TRACE() << "min_expo = " << min_expo << " (s)";

            // reload initial value of exposure time
            Camera_->ExposureTimeBaseAbs.SetValue(initial_base);
            Camera_->ExposureTimeRaw.SetValue(initial_raw);

            DEB_TRACE() << "initial value of exposure time was reloaded";
        }
        else
        {
            min_expo = Camera_->ExposureTimeAbs.GetMin()*1e-6;
            max_expo = Camera_->ExposureTimeAbs.GetMax()*1e-6;
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
void Camera::getLatTimeRange(double& min_lat, double& max_lat) const
{
    DEB_MEMBER_FUNCT();
    try
    {
        min_lat = 0;
        double minAcqFrameRate = Camera_->AcquisitionFrameRateAbs.GetMin();
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
    AutoMutex aLock(m_cond.mutex());
    status = m_status;
    //Check if camera is not waiting for trigger
    if((status == Camera::Exposure || status == Camera::Readout) && 
       m_trigger_mode == IntTrigMult)
      {
	// Check the frame start trigger acquisition status
	// Set the acquisition status selector
	Camera_->AcquisitionStatusSelector.SetValue
	  (AcquisitionStatusSelector_FrameTriggerWait);
	// Read the acquisition status
	bool IsWaitingForFrameTrigger = Camera_->AcquisitionStatus.GetValue();
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
void Camera::getFrameRate(double& frame_rate) const
{
    DEB_MEMBER_FUNCT();
    try
    {
        frame_rate = static_cast<double>(Camera_->ResultingFrameRateAbs.GetValue());        
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
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(set_roi);
    try
    {
        if (set_roi.isActive())
        {
	    // Taking care of the value increment, round up the ROI, 
	    // some cameras have increment > 1, ie. acA1920-50gm (Sony CMOS) Inc is 4 for width and offset X
	    const Roi rup_roi (
			       ceil(set_roi.getTopLeft().x*1.0/Camera_->OffsetX.GetInc()) * Camera_->OffsetX.GetInc(),
			       ceil(set_roi.getTopLeft().y*1.0/Camera_->OffsetY.GetInc()) * Camera_->OffsetY.GetInc(),
			       ceil(set_roi.getSize().getWidth()*1.0/Camera_->Width.GetInc()) * Camera_->Width.GetInc(),
			       ceil(set_roi.getSize().getHeight()*1.0/Camera_->Height.GetInc()) * Camera_->Height.GetInc());
	    
	    hw_roi = rup_roi;

	    // size at minimum 
            const Size& aSetRoiSize = hw_roi.getSize();
            Size aRoiSize = Size(max(aSetRoiSize.getWidth(),
				     int(Camera_->Width.GetMin())),
                                 max(aSetRoiSize.getHeight(),
				     int(Camera_->Height.GetMin())));
            hw_roi.setSize(aRoiSize);
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
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(ask_roi);
    Roi r;    
    try
    {
        //- backup old roi, in order to rollback if error
        getRoi(r);
        if(r == ask_roi) return;
        
        //- first reset the ROI
        Camera_->OffsetX.SetValue(Camera_->OffsetX.GetMin());
        Camera_->OffsetY.SetValue(Camera_->OffsetY.GetMin());
        Camera_->Width.SetValue(Camera_->Width.GetMax());
        Camera_->Height.SetValue(Camera_->Height.GetMax());
        
        Roi fullFrame(  Camera_->OffsetX.GetMin(),
			Camera_->OffsetY.GetMin(),
			Camera_->Width.GetMax(),
			Camera_->Height.GetMax());

        if(ask_roi.isActive() && fullFrame != ask_roi)
	{
            //- then fix the new ROI
            Camera_->Width.SetValue(ask_roi.getSize().getWidth());
            Camera_->Height.SetValue(ask_roi.getSize().getHeight());
            Camera_->OffsetX.SetValue(ask_roi.getTopLeft().x);
            Camera_->OffsetY.SetValue(ask_roi.getTopLeft().y);
        }
    }
    catch (GenICam::GenericException &e)
    {
        try
        {
            //-  rollback the old roi
            Camera_->Width.SetValue( r.getSize().getWidth());
            Camera_->Height.SetValue(r.getSize().getHeight());
            Camera_->OffsetX.SetValue(r.getTopLeft().x);
            Camera_->OffsetY.SetValue(r.getTopLeft().y);
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
    DEB_MEMBER_FUNCT();
    try
    {
        Roi  r( static_cast<int>(Camera_->OffsetX()),
                static_cast<int>(Camera_->OffsetY()),
                static_cast<int>(Camera_->Width())    ,
                static_cast<int>(Camera_->Height())
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
    try
    {
        int x = aBin.getX();
        if (x > Camera_->BinningHorizontal.GetMax())
            x = Camera_->BinningHorizontal.GetMax();

        int y = aBin.getY();
        if (y > Camera_->BinningVertical.GetMax())
            y = Camera_->BinningVertical.GetMax();
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
    try
    {
        Camera_->BinningVertical.SetValue(aBin.getY());
        Camera_->BinningHorizontal.SetValue(aBin.getX());
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
    try
    {
      aBin = Bin(Camera_->BinningHorizontal.GetValue(), Camera_->BinningVertical.GetValue());
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
bool Camera::isBinningAvailable() const
{
    DEB_MEMBER_FUNCT();
    bool isAvailable = false;
    try
    {
      isAvailable = (GenApi::IsAvailable(Camera_->BinningVertical) &&
		     GenApi::IsAvailable(Camera_->BinningHorizontal));
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
bool Camera::isRoiAvailable() const
{
  DEB_MEMBER_FUNCT();
  bool isAvailable = false;
  try
    {
      isAvailable = (GenApi::IsAvailable(Camera_->OffsetX) && GenApi::IsWritable(Camera_->OffsetX) &&
		     GenApi::IsAvailable(Camera_->OffsetY) && GenApi::IsWritable(Camera_->OffsetY) &&
		     GenApi::IsAvailable(Camera_->Width) && GenApi::IsWritable(Camera_->Width) &&
		     GenApi::IsAvailable(Camera_->Height) && GenApi::IsWritable(Camera_->Height));
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
    try
    {
        Camera_->GevSCPSPacketSize.SetValue(isize);
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
    try
    {
        isize = Camera_->GevSCPSPacketSize.GetValue();
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
    try
    {
        Camera_->GevSCPD.SetValue(ipd);
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
    try
    {
        ipd = Camera_->GevSCPD.GetValue();
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
bool Camera::isGainAvailable() const
{
    return GenApi::IsAvailable(Camera_->GainRaw);
}


//-----------------------------------------------------
// isAutoGainAvailable
//-----------------------------------------------------
bool Camera::isAutoGainAvailable() const
{
    return GenApi::IsAvailable(Camera_->GainAuto);
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getBandwidthAssigned(int& ipd)
{
    DEB_MEMBER_FUNCT();
    try
    {
        ipd = Camera_->GevSCBWA.GetValue();
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
    try
    {
        ipd = Camera_->GevSCDMT.GetValue();
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
    try
    {
        ipd = Camera_->GevSCDCT.GetValue();
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}    

//-----------------------------------------------------
// isTemperatureAvailable
//-----------------------------------------------------
bool Camera::isTemperatureAvailable() const
{
    return GenApi::IsAvailable(Camera_->TemperatureAbs);
}

//-----------------------------------------------------
//
//-----------------------------------------------------

void Camera::getTemperature(double& temperature)
{
    DEB_MEMBER_FUNCT();
    try
    {
        // If the parameter TemperatureAbs is available for this camera
        if (GenApi::IsAvailable(Camera_->TemperatureAbs))
            temperature = Camera_->TemperatureAbs.GetValue();
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
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(auto_gain);
    try
    {
        if (GenApi::IsAvailable(Camera_->GainAuto) && GenApi::IsAvailable(Camera_->GainSelector))
        {
            if (!auto_gain)
            {
                Camera_->GainAuto.SetValue(GainAuto_Off);
                Camera_->GainSelector.SetValue(GainSelector_All);
            }
            else
            {
                Camera_->GainAuto.SetValue(GainAuto_Continuous);
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
void Camera::getAutoGain(bool& auto_gain) const
{
    DEB_MEMBER_FUNCT();
    try
    {
        if (GenApi::IsAvailable(Camera_->GainAuto))
        {
            auto_gain = Camera_->GainAuto.GetValue();
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
    try
    {
        // you want to set the gain, remove autogain
        if (GenApi::IsAvailable(Camera_->GainAuto))
        {		
			setAutoGain(false);
		}
		
        if (GenApi::IsWritable(Camera_->GainRaw) && GenApi::IsAvailable(Camera_->GainRaw))
        {

            int low_limit = Camera_->GainRaw.GetMin();
            DEB_TRACE() << "low_limit = " << low_limit;

            int hight_limit = Camera_->GainRaw.GetMax();
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
            Camera_->GainRaw.SetValue(gain_raw);
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
void Camera::getGain(double& gain) const
{
    DEB_MEMBER_FUNCT();
    try
    {
        if (GenApi::IsAvailable(Camera_->GainRaw))
        {
            int gain_raw = Camera_->GainRaw.GetValue();
            int low_limit = Camera_->GainRaw.GetMin();
            int hight_limit = Camera_->GainRaw.GetMax();

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
    try
    {
        Camera_->GevSCFTD.SetValue(ftd);
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
}

//--------------------------------------------------------
//
//-----------------------------------------------------
void Camera::setAcquisitionFrameRateEnable(bool AFRE)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(AFRE);
    try
    {
        Camera_->AcquisitionFrameRateEnable.SetValue(AFRE);
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
}

//--------------------------------------------------------
//
//-----------------------------------------------------
void Camera::getAcquisitionFrameRateEnable(bool& AFRE) const
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(AFRE);
    try
    {
        if (GenApi::IsAvailable(Camera_->AcquisitionFrameRateEnable))
        {
            AFRE  = Camera_->AcquisitionFrameRateEnable.GetValue();
        }
        else
        {
            AFRE = false;
//			THROW_HW_ERROR(Error)<<"AcquisitionFrameRateEnable Parameter is not Available !";
        }
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
        DEB_RETURN() << DEB_VAR1(AFRE);

}
//--------------------------------------------------------
//
//-----------------------------------------------------
void Camera::setAcquisitionFrameRateAbs(int AFRA)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(AFRA);
    try
    {
        Camera_->AcquisitionFrameRateAbs.SetValue(AFRA);
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
}

//--------------------------------------------------------
//
//-----------------------------------------------------
void Camera::getAcquisitionFrameRateAbs(int& AFRA) const
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(AFRA);
    try
    {
        if (GenApi::IsAvailable(Camera_->AcquisitionFrameRateAbs))
        {
        AFRA  = Camera_->AcquisitionFrameRateAbs.GetValue();
        }
        else
        {
            AFRA = false;
//			THROW_HW_ERROR(Error)<<"AcquisitionFrameRateAbs Parameter is not Available !";
        }
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
        DEB_RETURN() << DEB_VAR1(AFRA);

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

  Camera_->LineSelector.SetValue(LineSelector_Out1);

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
  Camera_->LineSource.SetValue(line_src);
}

void Camera::getOutput1LineSource(Camera::LineSource& source) const
{
  DEB_MEMBER_FUNCT();

  Camera_->LineSelector.SetValue(LineSelector_Out1);
  switch(Camera_->LineSource.GetValue())
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


//-----------------------------------------------------
// Set the output line1 to a user value: True on False
// the output line1 source can be overwritten by calling
// setOutput1LineSource()
//-----------------------------------------------------
void Camera::setUserOutputLine1(bool value)
{
  DEB_MEMBER_FUNCT();
  // set the I/O output1 to be set by the UserOutput value
  Camera_->LineSelector.SetValue(LineSelector_Out1);
  Camera_->LineSource.SetValue(LineSource_UserOutput);
  
  Camera_->UserOutputSelector.SetValue(UserOutputSelector_UserOutput1);
  Camera_->UserOutputValue.SetValue(value);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getUserOutputLine1(bool &value) const
{
  DEB_MEMBER_FUNCT();
  Camera_->UserOutputSelector.SetValue(UserOutputSelector_UserOutput1);
  value = Camera_->UserOutputValue.GetValue();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setAcquisitionFrameCount(int AFC)
{
    DEB_MEMBER_FUNCT();
    try
    {
        // If the parameter AcquisitionFrameCount is available for this camera
        if (GenApi::IsAvailable(Camera_->AcquisitionFrameCount))
            Camera_->AcquisitionFrameCount.SetValue(AFC);
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getAcquisitionFrameCount(int& AFC) const
{
    DEB_MEMBER_FUNCT();
    try
    {
        // If the parameter AcquisitionFrameCount is available for this camera
        if (GenApi::IsAvailable(Camera_->AcquisitionFrameCount))
           AFC = Camera_->AcquisitionFrameCount.GetValue();
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
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


//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setTestImageSelector(TestImageSelector set)
{
    DEB_MEMBER_FUNCT();
    try
    {
        Basler_GigECamera::TestImageSelectorEnums test =
            static_cast<Basler_GigECamera::TestImageSelectorEnums>(set);

        // If the parameter TestImage is available for this camera
        if (GenApi::IsAvailable(Camera_->TestImageSelector))
            Camera_->TestImageSelector.SetValue(test);
    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getTestImageSelector(TestImageSelector& set) const
{
    DEB_MEMBER_FUNCT();
    try
    {
        Basler_GigECamera::TestImageSelectorEnums test;

        // If the parameter TestImage is available for this camera
        if (GenApi::IsAvailable(Camera_->TestImageSelector))
            test = Camera_->TestImageSelector.GetValue();

        set = static_cast<TestImageSelector>(test);

    }
    catch (GenICam::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

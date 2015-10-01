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
Camera::Camera(const std::string& camera_ip,int packet_size,int receive_priority)
        : m_nb_frames(1),
          m_status(Ready),
          m_wait_flag(true),
          m_quit(false),
          m_thread_running(true),
          m_image_number(0),
          m_exp_time(1.),
          m_timeout(DEFAULT_TIME_OUT),
          m_latency_time(0.),
          Camera_(NULL),
          StreamGrabber_(NULL),
          m_receive_priority(receive_priority),
	  m_video_flag_mode(false),
	  m_video(NULL)
{
    DEB_CONSTRUCTOR();
    m_camera_ip = camera_ip;
    try
    {
        Pylon::PylonInitialize( );
        // Create the transport layer object needed to enumerate or
        // create a camera object of type Camera_t::DeviceClass()
        DEB_TRACE() << "Create a camera object of type Camera_t::DeviceClass()";
        CTlFactory& TlFactory = CTlFactory::GetInstance();

        // camera_ip is not really necessarily an IP, it may also be a DNS name
        // pylon_camera_ip IS an IP
        Pylon::String_t pylon_camera_ip(_get_ip_addresse(m_camera_ip.c_str()));

        //- Find the Pylon device thanks to its IP Address
        CBaslerGigEDeviceInfo di;
        di.SetIpAddress( pylon_camera_ip);
        DEB_TRACE() << "Create the Pylon device attached to ip address : " << DEB_VAR1(m_camera_ip);
        IPylonDevice* device = TlFactory.CreateDevice( di);
        if (!device)
        {
            Pylon::PylonTerminate( );
            THROW_HW_ERROR(Error) << "Unable to find camera with selected IP!";
        }

        //- Create the Basler Camera object
        DEB_TRACE() << "Create the Camera object corresponding to the created Pylon device";
        Camera_ = new Camera_t(device);
        if(!Camera_)
        {
            Pylon::PylonTerminate( );
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
        static const char* PixelFormatStr[] = {"BayerRG16","BayerBG16",
					       "BayerRG12","BayerBG12",
					       "BayerRG8","BayerBG8",
					       "Mono16", "Mono12", "Mono8",NULL};
        bool formatSetFlag = false;
        for(const char** pt = PixelFormatStr;*pt;++pt)
        {
            GenApi::IEnumEntry *anEntry = Camera_->PixelFormat.GetEntryByName(*pt);
            if(anEntry && GenApi::IsAvailable(anEntry))
            {
                formatSetFlag = true;
		m_color_flag = *pt[0] == 'B';
		Camera_->PixelFormat.SetIntValue(anEntry->GetValue());
                DEB_TRACE() << "Set pixel format to " << *pt;
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

        // Get the image buffer size
        DEB_TRACE() << "Get the image buffer size";
        ImageSize_ = (size_t)(Camera_->PayloadSize.GetValue());
           
        WaitObject_ = WaitObjectEx::Create();
    
        m_acq_thread = new _AcqThread(*this);
        m_acq_thread->start();
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        Pylon::PylonTerminate( );
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
    if(m_color_flag)
      _initColorStreamGrabber(true);
    else
      {
	for(int i = 0;i < NB_COLOR_BUFFER;++i)
	  m_color_buffer[i] = NULL;
      }
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
        Pylon::PylonTerminate( );
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
}

void Camera::prepareAcq()
{
    DEB_MEMBER_FUNCT();
    m_image_number=0;

    if(m_video_flag_mode)
      return;			// Nothing to do if color camera

    try
    {
	_freeStreamGrabber();
        // Get the first stream grabber object of the selected camera
        DEB_TRACE() << "Get the first stream grabber object of the selected camera";
        StreamGrabber_ = new Camera_t::StreamGrabber_t(Camera_->GetStreamGrabber(0));
	//Change priority to m_receive_priority
	if(m_receive_priority > 0)
	  {
	    StreamGrabber_->ReceiveThreadPriorityOverride.SetValue(true);
	    StreamGrabber_->ReceiveThreadPriority.SetValue(m_receive_priority);
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
        // Let the camera acquire images continuously ( Acquisiton mode equals Continuous! )
        DEB_TRACE() << "Let the camera acquire images continuously";

	if(m_video)
	  m_video->getBuffer().setStartTimestamp(Timestamp::now());
	else
	  m_buffer_ctrl_obj.getBuffer().setStartTimestamp(Timestamp::now());

        Camera_->AcquisitionStart.Execute();

	//Start acqusition thread
	AutoMutex aLock(m_cond.mutex());
        m_wait_flag = false;
        m_cond.broadcast();
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }
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

void Camera::_initColorStreamGrabber(bool allocFlag)
{
  DEB_MEMBER_FUNCT();

  StreamGrabber_ = new Camera_t::StreamGrabber_t(Camera_->GetStreamGrabber(0));
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
      if(allocFlag)
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
    try
    {
        PixelFormatEnums ps = Camera_->PixelFormat.GetValue();
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

    try
    {        
        if ( mode == IntTrig )
        {
            //- INTERNAL 
            this->Camera_->TriggerSelector.SetValue( TriggerSelector_AcquisitionStart );
            this->Camera_->TriggerMode.SetValue( TriggerMode_Off );
                        
            GenApi::IEnumEntry *enumEntryFrameStart = Camera_->TriggerSelector.GetEntryByName("FrameStart");                    
            if(enumEntryFrameStart && GenApi::IsAvailable(enumEntryFrameStart))            
                this->Camera_->TriggerSelector.SetValue( TriggerSelector_FrameStart );
            
            this->Camera_->TriggerMode.SetValue( TriggerMode_Off );
            this->Camera_->ExposureMode.SetValue(ExposureMode_Timed);
        }
        else if ( mode == ExtGate )
        {
            //- EXTERNAL - TRIGGER WIDTH
            this->Camera_->TriggerSelector.SetValue( TriggerSelector_AcquisitionStart );
            this->Camera_->TriggerMode.SetValue( TriggerMode_On );
            
            GenApi::IEnumEntry *enumEntryFrameStart = Camera_->TriggerSelector.GetEntryByName("FrameStart");                    
            if(enumEntryFrameStart && GenApi::IsAvailable(enumEntryFrameStart))                    
                this->Camera_->TriggerSelector.SetValue( TriggerSelector_FrameStart );
            
            this->Camera_->TriggerMode.SetValue( TriggerMode_On );
            this->Camera_->AcquisitionFrameRateEnable.SetValue( false );
            this->Camera_->ExposureMode.SetValue( ExposureMode_TriggerWidth );
        }        
        else //ExtTrigSingle
        {
            //- EXTERNAL - TIMED
            
            this->Camera_->TriggerSelector.SetValue( TriggerSelector_AcquisitionStart );
            this->Camera_->TriggerMode.SetValue( TriggerMode_On );
            
            GenApi::IEnumEntry *enumEntryFrameStart = Camera_->TriggerSelector.GetEntryByName("FrameStart");                    
            if(enumEntryFrameStart && GenApi::IsAvailable(enumEntryFrameStart))                     
                this->Camera_->TriggerSelector.SetValue( TriggerSelector_FrameStart );
            this->Camera_->TriggerMode.SetValue( TriggerMode_On );
            this->Camera_->AcquisitionFrameRateEnable.SetValue( false );
            this->Camera_->ExposureMode.SetValue( ExposureMode_Timed );
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
void Camera::getTrigMode(TrigMode& mode)
{
    DEB_MEMBER_FUNCT();
    int frameStart = TriggerMode_Off, acqStart = TriggerMode_Off, expMode;

    try
    {              
        this->Camera_->TriggerSelector.SetValue( TriggerSelector_AcquisitionStart );
        acqStart =  this->Camera_->TriggerMode.GetValue();

        GenApi::IEnumEntry *enumEntryFrameStart = Camera_->TriggerSelector.GetEntryByName("FrameStart");  
        if(enumEntryFrameStart && GenApi::IsAvailable(enumEntryFrameStart))            
        {
            this->Camera_->TriggerSelector.SetValue( TriggerSelector_FrameStart );
            frameStart =  this->Camera_->TriggerMode.GetValue();
        }
        
        expMode = this->Camera_->ExposureMode.GetValue();        
        if ((acqStart ==  TriggerMode_Off) && (frameStart ==  TriggerMode_Off))
            mode = IntTrig;
        else if (expMode == ExposureMode_TriggerWidth)
            mode = ExtGate;
        else //ExposureMode_Timed
            mode = ExtTrigSingle;
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }        
   	
    DEB_RETURN() << DEB_VAR4(mode,acqStart, frameStart, expMode);    
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
    DEB_RETURN() << DEB_VAR1(DEB_HEX(status));
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
            const Size& aSetRoiSize = set_roi.getSize();
            Size aRoiSize = Size(max(aSetRoiSize.getWidth(),
				     int(Camera_->Width.GetMin())),
                                 max(aSetRoiSize.getHeight(),
				     int(Camera_->Height.GetMin())));
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
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(ask_roi);
    Roi set_roi;
    checkRoi(ask_roi,set_roi);
    Roi r;    
    try
    {
        //- backup old roi, in order to rollback if error
        getRoi(r);
        if(r == set_roi) return;
        
        //- first reset the ROI
        Camera_->OffsetX.SetValue(Camera_->OffsetX.GetMin());
        Camera_->OffsetY.SetValue(Camera_->OffsetY.GetMin());
        Camera_->Width.SetValue(Camera_->Width.GetMax());
        Camera_->Height.SetValue(Camera_->Height.GetMax());
        
        Roi fullFrame(  Camera_->OffsetX.GetMin(),
						Camera_->OffsetY.GetMin(),
						Camera_->Width.GetMax(),
						Camera_->Height.GetMax());
        
        if(set_roi.isActive() && fullFrame != set_roi)
        {
            //- then fix the new ROI
            Camera_->Width.SetValue( set_roi.getSize().getWidth());
            Camera_->Height.SetValue(set_roi.getSize().getHeight());
            Camera_->OffsetX.SetValue(set_roi.getTopLeft().x);
            Camera_->OffsetY.SetValue(set_roi.getTopLeft().y);
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
    _freeStreamGrabber();
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
    	aBin = Bin(Camera_->BinningVertical.GetValue(), Camera_->BinningHorizontal.GetValue());
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

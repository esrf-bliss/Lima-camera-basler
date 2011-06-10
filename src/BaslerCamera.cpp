#include "BaslerCamera.h"
#include <sstream>
#include <iostream>
#include <string>
#include <math.h>
using namespace lima;
using namespace lima::Basler;
using namespace std;

// Buffers for grabbing
static const uint32_t c_nBuffers 			= 1;
#ifdef LESSDEPENDENCY
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

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
#endif
#ifndef LESSDEPENDENCY
#define SET_STATUS(camera_status) m_status = camera_status
#define GET_STATUS() m_status
#else
#define SET_STATUS(camera_status) setStatus(camera_status)
#define GET_STATUS() Camera::Status(CmdThread::getStatus())
#endif 

//---------------------------
//- Ctor
//---------------------------
Camera::Camera(const std::string& camera_ip)
		: m_buffer_cb_mgr(m_buffer_alloc_mgr),
		  m_buffer_ctrl_mgr(m_buffer_cb_mgr),
		  m_image_number(0),
		  m_stop_already_done(true),
		  m_exp_time(1.),
		  pTl_(NULL),
		  Camera_(NULL),
		  StreamGrabber_(NULL)
{
	DEB_CONSTRUCTOR();
	m_camera_ip = camera_ip;
	try
    {		
		SET_STATUS(Camera::Ready);		
		Pylon::PylonInitialize( );
        // Create the transport layer object needed to enumerate or
        // create a camera object of type Camera_t::DeviceClass()
		DEB_TRACE() << "Create a camera object of type Camera_t::DeviceClass()";
        pTl_ = CTlFactory::GetInstance().CreateTl(Camera_t::DeviceClass());

        // Exit application if the specific transport layer is not available
        if (! pTl_)
        {
			DEB_ERROR() << "Failed to create transport layer!";			
			Pylon::PylonTerminate( );			
            throw LIMA_HW_EXC(Error, "Failed to create transport layer!");
        }

        // Get all attached cameras and exit application if no camera is found
		DEB_TRACE() << "Get all attached cameras, EnumerateDevices";
        if (0 == pTl_->EnumerateDevices(devices_))
        {
			DEB_ERROR() << "No camera present!";
			Pylon::PylonTerminate( );			
            throw LIMA_HW_EXC(Error, "No camera present!");
        }

		// Find the device with an IP corresponding to the one given in property
		Pylon::DeviceInfoList_t::const_iterator it;		
		
		// camera_ip is not really necessarily an IP, it may also be a DNS name
		// pylon_camera_ip IS an IP
#ifndef LESSDEPENDENCY
		Pylon::String_t pylon_camera_ip(yat::Address(m_camera_ip, 0).get_ip_address().c_str());
#else
		Pylon::String_t pylon_camera_ip(_get_ip_addresse(m_camera_ip.c_str()));
#endif
		for (it = devices_.begin(); it != devices_.end(); it++)
		{
			const Camera_t::DeviceInfo_t& gige_device_info = static_cast<const Camera_t::DeviceInfo_t&>(*it);
			Pylon::String_t current_ip = gige_device_info.GetIpAddress();
			DEB_TRACE() << "Found cam with Ip <" << DEB_VAR1(current_ip) << '>';
			//if Ip camera is found.
			if (current_ip == pylon_camera_ip)
			{
				m_detector_type  = gige_device_info.GetVendorName();
				m_detector_model = gige_device_info.GetModelName();
				break;
			}
		}
		if (it == devices_.end())
		{
			DEB_ERROR() << "Camera " << m_camera_ip<< " not found.";
			Pylon::PylonTerminate( );
			throw LIMA_HW_EXC(Error, "Camera not found!");
		}
		DEB_TRACE() << DEB_VAR2(m_detector_type,m_detector_model);
		
		// Create the camera object of the first available camera
		// The camera object is used to set and get all available
		// camera features.
		DEB_TRACE() << "Create the camera object attached to ip address : " << DEB_VAR1(camera_ip);
		Camera_ = new Camera_t(pTl_->CreateDevice(*it));
		
		if( !Camera_->GetDevice() )
		{
			DEB_ERROR() <<"Unable to get the camera from transport_layer!";
			Pylon::PylonTerminate( );
			throw LIMA_HW_EXC(Error, "Unable to get the camera from transport_layer!");
		}
	  
        
        // Open the camera
        Camera_->Open();

        // Get the first stream grabber object of the selected camera
		DEB_TRACE() << "Get the first stream grabber object of the selected camera";
		StreamGrabber_ = new Camera_t::StreamGrabber_t(Camera_->GetStreamGrabber(0));
        // Open the stream grabber
		DEB_TRACE() << "Open the stream grabber";
        StreamGrabber_->Open();
		if(!StreamGrabber_->IsOpen())
		{
			DEB_ERROR() <<"Unable to open the steam grabber!";
			throw LIMA_HW_EXC(Error,"Unable to open the steam grabber!");
		}

        // Set the image format and AOI
		DEB_TRACE() << "Set the image format and AOI";
		static const char* PixelFormatStr[] = {"Mono16", "Mono12", "Mono8",NULL};
		bool formatSetFlag = false;
		for(const char** pt = PixelFormatStr;*pt;++pt)
		{
			GenApi::IEnumEntry *anEntry = Camera_->PixelFormat.GetEntryByName(*pt);
			if(anEntry && GenApi::IsAvailable(anEntry))
			{
				formatSetFlag = true;
				Camera_->PixelFormat.SetIntValue(Camera_->PixelFormat.GetEntryByName(*pt)->GetValue());
				DEB_TRACE() << "Set pixel format to " << *pt;
				break;
			}
		}
		if(!formatSetFlag)
		{
			DEB_ERROR() << "Unable to set PixelFormat for the camera!";
			throw LIMA_HW_EXC(Error, "Unable to set PixelFormat for the camera!");
		}

        // Set the camera to continuous frame mode
		DEB_TRACE() << "Set the camera to continuous frame mode";
        Camera_->TriggerSelector.SetValue(TriggerSelector_AcquisitionStart);
        Camera_->AcquisitionMode.SetValue(AcquisitionMode_Continuous);
		////Camera_->ExposureAuto.SetValue(ExposureAuto_Off);
        // Get the image buffer size
		DEB_TRACE() << "Get the image buffer size";
		ImageSize_ = (size_t)(Camera_->PayloadSize.GetValue());
       
        // We won't use image buffers greater than ImageSize
		DEB_TRACE() << "We won't use image buffers greater than ImageSize";
        StreamGrabber_->MaxBufferSize.SetValue((const size_t)ImageSize_);

        // We won't queue more than c_nBuffers image buffers at a time
		DEB_TRACE() << "We won't queue more than c_nBuffers image buffers at a time";
        StreamGrabber_->MaxNumBuffer.SetValue(c_nBuffers);

#ifdef LESSDEPENDENCY
	start();
#endif
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << e.GetDescription();
		Pylon::PylonTerminate( );
        throw LIMA_HW_EXC(Error, e.GetDescription());
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
		// Close stream grabber
		if(StreamGrabber_->IsOpen())
		{	DEB_TRACE() << "Close stream grabber";
			StreamGrabber_->Close();
		}
	
		if(Camera_->IsOpen())
		{
			// Close camera
			DEB_TRACE() << "Close camera";
			Camera_->Close();
		}
		
		// Free all resources used for grabbing
		DEB_TRACE() << "Free all resources used for grabbing";
		StreamGrabber_->FinishGrab();
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << e.GetDescription();
		Pylon::PylonTerminate( );
        throw LIMA_HW_EXC(Error, e.GetDescription());
    }
	delete pTl_;
	delete Camera_;
	delete StreamGrabber_;	
	Pylon::PylonTerminate( );
}

//---------------------------
//- Camera::start()
//---------------------------
void Camera::startAcq()
{
	DEB_MEMBER_FUNCT();
    try
    {
		m_image_number=0;		
		// Allocate all resources for grabbing. Critical parameters like image
		// size now must not be changed until FinishGrab() is called.
		DEB_TRACE() << "Allocate all resources for grabbing, PrepareGrab";
		StreamGrabber_->PrepareGrab();
	
		// Buffers used for grabbing must be registered at the stream grabber.
		// The registration returns a handle to be used for queuing the buffer.
		DEB_TRACE() << "Buffers used for grabbing must be registered at the stream grabber";
		for (uint32_t i = 0; i < c_nBuffers; ++i)
		{
			CGrabBuffer *pGrabBuffer = new CGrabBuffer((const size_t)ImageSize_);
			pGrabBuffer->SetBufferHandle(StreamGrabber_->RegisterBuffer(pGrabBuffer->GetBufferPointer(), (const size_t)ImageSize_));
	
			// Put the grab buffer object into the buffer list
			DEB_TRACE() << "Put the grab buffer object into the buffer list";
			BufferList_.push_back(pGrabBuffer);
		}
	
		DEB_TRACE() << "Handle to be used for queuing the buffer";
		for (vector<CGrabBuffer*>::const_iterator x = BufferList_.begin(); x != BufferList_.end(); ++x)
		{
			// Put buffer into the grab queue for grabbing
			DEB_TRACE() << "Put buffer into the grab queue for grabbing";
			StreamGrabber_->QueueBuffer((*x)->GetBufferHandle(), NULL);
		}
		
		// Let the camera acquire images continuously ( Acquisiton mode equals Continuous! )
		DEB_TRACE() << "Let the camera acquire images continuously";
		Camera_->AcquisitionStart.Execute();
#ifndef LESSDEPENDENCY
		this->post(new yat::Message(BASLER_START_MSG), kPOST_MSG_TMO);
#else
		sendCmd(BASLER_START_MSG);
		waitNotStatus(Ready); 
#endif
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << e.GetDescription();
        throw LIMA_HW_EXC(Error, e.GetDescription());
    }
}

//---------------------------
//- Camera::stop()
//---------------------------
void Camera::stopAcq()
{
	DEB_MEMBER_FUNCT();
	{
#ifndef LESSDEPENDENCY
		this->post(new yat::Message(BASLER_STOP_MSG), kPOST_MSG_TMO);
#else
		/** - do we really need to lock before posting the message???
		    - I really don't like to have an asynchronous stop so put the synchro...
		      So why we need a message to stop???
		*/
		sendCmd(BASLER_STOP_MSG);
		waitStatus(Ready);
#endif
	}
}
//---------------------------
//- Camera::FreeImage()
//---------------------------
void Camera::FreeImage()
{
  DEB_MEMBER_FUNCT();
	try
	{
		SET_STATUS(Camera::Ready);
		if(!m_stop_already_done)
		{
			m_stop_already_done = true;
			// Get the pending buffer back (You are not allowed to deregister
			// buffers when they are still queued)
			StreamGrabber_->CancelGrab();
	
			// Get all buffers back
			for (GrabResult r; StreamGrabber_->RetrieveResult(r););
			
			// Stop acquisition
			DEB_TRACE() << "Stop acquisition";
			Camera_->AcquisitionStop.Execute();
			
			// Clean up
		
			// You must deregister the buffers before freeing the memory
			DEB_TRACE() << "Must deregister the buffers before freeing the memory";
			for (vector<CGrabBuffer*>::iterator it = BufferList_.begin(); it != BufferList_.end(); it++)
			{
				StreamGrabber_->DeregisterBuffer((*it)->GetBufferHandle());
				delete *it;
				*it = NULL;
			}
			BufferList_.clear();
			// Free all resources used for grabbing
			DEB_TRACE() << "Free all resources used for grabbing";
			StreamGrabber_->FinishGrab();
		}
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << e.GetDescription();
        throw LIMA_HW_EXC(Error, e.GetDescription());
    }	
}

//---------------------------
//- static bool acq_continue()
//---------------------------
#ifdef LESSDEPENDENCY
static bool acq_continue(int cmd,int status)
{
  return cmd != Camera::BASLER_STOP_MSG;
}
#endif

//---------------------------
//- Camera::GetImage()
//---------------------------
void Camera::GetImage()
{
  DEB_MEMBER_FUNCT();
	try
	{
		if(m_stop_already_done)
			return;		
		StdBufferCbMgr& buffer_mgr = m_buffer_cb_mgr;
		
		int local_exp_time = int(m_exp_time + 1.) * 1000;
		DEB_TRACE() << DEB_VAR1(local_exp_time);
		SET_STATUS(Camera::Exposure);
		if (StreamGrabber_->GetWaitObject().Wait(local_exp_time))
		{
			// Get the grab result from the grabber's result queue
			GrabResult Result;
			StreamGrabber_->RetrieveResult(Result);
			bool continueAcq = false;
			if (Grabbed == Result.Status())
			{
				// Grabbing was successful, process image
				SET_STATUS(Camera::Readout);
				DEB_TRACE()  << "image#" << DEB_VAR1(m_image_number) <<" acquired !";
				int buffer_nb, concat_frame_nb;		
				buffer_mgr.setStartTimestamp(Timestamp::now());
				buffer_mgr.acqFrameNb2BufferNb(m_image_number, buffer_nb, concat_frame_nb);
				void *ptr = buffer_mgr.getBufferPtr(buffer_nb,   concat_frame_nb);
				memcpy((int16_t *)ptr,(uint16_t *)( Result.Buffer()),Camera_->Width()*Camera_->Height()*2);
				
				HwFrameInfoType frame_info;
				frame_info.acq_frame_nb = m_image_number;
				continueAcq = buffer_mgr.newFrameReady(frame_info);
				DEB_TRACE() << DEB_VAR1(continueAcq);
				// Reuse the buffer for grabbing the next image
				if (!m_nb_frames || m_image_number < int(m_nb_frames - c_nBuffers))
					StreamGrabber_->QueueBuffer(Result.Handle(), NULL);
				m_image_number++;
			}
			else if (Failed == Result.Status())
			{
				// Error handling
				DEB_ERROR() << "No image acquired!"
							<< " Error code : 0x" << DEB_VAR1(hex) << " " << Result.GetErrorCode()
						    << " Error description : " << Result.GetErrorDescription();

				// Reuse the buffer for grabbing the next image
				if (!m_nb_frames || m_image_number < int(m_nb_frames - c_nBuffers))
					StreamGrabber_->QueueBuffer(Result.Handle(), NULL);
			
				
				if(!m_nb_frames) //Do not stop acquisition in "live" mode, just IGNORE  error
				{
					continueAcq = true;
				}
				else			//in "snap" mode , acquisition must be stopped
				{
					SET_STATUS(Camera::Fault);
					return;
				}
			}
			
			{
#ifndef LESSDEPENDENCY
				yat::MutexLock scoped_lock(lock_);
#endif
				// if nb acquired image < requested frames
				if (continueAcq &&(!m_nb_frames || m_image_number<m_nb_frames))
				{
					// get the next image
#ifndef LESSDEPENDENCY
					this->post(new yat::Message(BASLER_GET_IMAGE_MSG), kPOST_MSG_TMO);	
#else
					/** It's not really optimal to repost a message but!!!
					    @todo try do it in a better way.
					*/
					sendCmdIf(BASLER_GET_IMAGE_MSG,acq_continue);
#endif
				}
				else
#ifdef LESSDEPENDENCY
				  sendCmd(BASLER_STOP_MSG);
#else
				  this->post(new yat::Message(BASLER_STOP_MSG),kPOST_MSG_TMO);
#endif
			}
		}
		else
		{
			// Timeout
			DEB_ERROR() << "Timeout occurred!";
			SET_STATUS(Camera::Fault);
		}
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << "GeniCam Error! "<< e.GetDescription();
        throw LIMA_HW_EXC(Error, e.GetDescription());
    }			
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getDetectorImageSize(Size& size)
{
	DEB_MEMBER_FUNCT();
	try
	{
		// get the max image size of the detector
		size= Size(Camera_->WidthMax(),Camera_->HeightMax());
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << e.GetDescription();
        throw LIMA_HW_EXC(Error, e.GetDescription());
    }			
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getPixelSize(double& size)
{
	DEB_MEMBER_FUNCT();
	size= PixelSize;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getImageType(ImageType& type)
{
	DEB_MEMBER_FUNCT();
	try
	{
		PixelSizeEnums ps = Camera_->PixelSize.GetValue();
		switch( ps )
		{
			case PixelSize_Bpp8:
				type= Bpp8;
			break;
		
			case PixelSize_Bpp12:
				type= Bpp12;
			break;
		
			case PixelSize_Bpp16: //- this is in fact 12 bpp inside a 16bpp image
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
		DEB_ERROR() << e.GetDescription();
        throw LIMA_HW_EXC(Error, e.GetDescription());
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
			case Bpp16:
				this->Camera_->PixelFormat.SetValue(PixelFormat_Mono16);
			break;
		
			default:
				throw LIMA_HW_EXC(Error, "Cannot change the format of the camera !");
			break;
		}
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << e.GetDescription();
        throw LIMA_HW_EXC(Error, e.GetDescription());
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
BufferCtrlMgr& Camera::getBufferMgr()
{
	return m_buffer_ctrl_mgr;
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
		  this->Camera_->TriggerMode.SetValue( TriggerMode_Off );
		  this->Camera_->ExposureMode.SetValue(ExposureMode_Timed);
		}
		else if ( mode == ExtGate )
		{
		  //- EXTERNAL - TRIGGER WIDTH
		  this->Camera_->TriggerMode.SetValue( TriggerMode_On );
		  this->Camera_->AcquisitionFrameRateEnable.SetValue( false );
		  this->Camera_->ExposureMode.SetValue( ExposureMode_TriggerWidth );
		}		
		else //ExtTrigSingle
		{
		  //- EXTERNAL - TIMED
		  this->Camera_->TriggerMode.SetValue( TriggerMode_On );
		  this->Camera_->AcquisitionFrameRateEnable.SetValue( false );
		  this->Camera_->ExposureMode.SetValue( ExposureMode_Timed );
		}
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << e.GetDescription();
        throw LIMA_HW_EXC(Error, e.GetDescription());
    }		
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getTrigMode(TrigMode& mode)
{
    DEB_MEMBER_FUNCT();
	try
	{
		if (this->Camera_->TriggerMode.GetValue() == TriggerMode_Off)
			mode = IntTrig;
		else if (this->Camera_->ExposureMode.GetValue() == ExposureMode_TriggerWidth)
			mode = ExtGate;
		else //ExposureMode_Timed
			mode = ExtTrigSingle;
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << e.GetDescription();
        throw LIMA_HW_EXC(Error, e.GetDescription());
    }		
    DEB_RETURN() << DEB_VAR1(mode);
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setExpTime(double exp_time)
{
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(exp_time);

	try
	{
      //see ImageGrabber for more details !!!
	  Camera_->ExposureTimeBaseAbs.SetValue(100.0); //- to be sure we can set the Raw setting on the full range (1 .. 4095)
      double raw = ::ceil( exp_time / 50 );
	  Camera_->ExposureTimeRaw.SetValue(static_cast<int>(raw));
      raw = static_cast<double>(Camera_->ExposureTimeRaw.GetValue());      
      Camera_->ExposureTimeBaseAbs.SetValue(1E6 * exp_time / Camera_->ExposureTimeRaw.GetValue());	
	  
	  m_exp_time = exp_time;
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << e.GetDescription();
        throw LIMA_HW_EXC(Error, e.GetDescription());
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
        throw LIMA_HW_EXC(Error, e.GetDescription());
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
	/////@@@@ TODO if necessary
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getLatTime(double& lat_time)
{
	DEB_MEMBER_FUNCT();
	/////@@@@ TODO if necessary
	lat_time = 1;
	DEB_RETURN() << DEB_VAR1(lat_time);
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
	status = GET_STATUS();
	DEB_RETURN() << DEB_VAR1(DEB_HEX(status));
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getFrameRate(double& frame_rate)
{
	DEB_MEMBER_FUNCT();
	try
	{
		frame_rate = static_cast<double>(Camera_->ResultingFrameRateAbs.GetValue());		
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
        throw LIMA_HW_EXC(Error, e.GetDescription());
    }		
	DEB_RETURN() << DEB_VAR1(frame_rate);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::checkRoi(const Roi& set_roi, Roi& hw_roi)
{
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(set_roi);
	Roi r;
	getRoi(r);
	if(!set_roi.isActive())
	{
		hw_roi = r;	
	}
	else
	{
		hw_roi = set_roi;
	}
	
	DEB_RETURN() << DEB_VAR1(hw_roi);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setRoi(const Roi& set_roi)
{
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(set_roi);
	Roi r;	
	try
	{
		//- backup old roi, in order to rollback if error
		getRoi(r);
		
		//- first reset the ROI
		Camera_->OffsetX.SetValue(Camera_->OffsetX.GetMin());
		Camera_->OffsetY.SetValue(Camera_->OffsetY.GetMin());
		Camera_->Width.SetValue(Camera_->Width.GetMax());
		Camera_->Height.SetValue(Camera_->Height.GetMax());
	
		//- then fix the new ROI
		Camera_->Width.SetValue( set_roi.getSize().getWidth());
		Camera_->Height.SetValue(set_roi.getSize().getHeight());	
		Camera_->OffsetX.SetValue(set_roi.getTopLeft().x);
		Camera_->OffsetY.SetValue(set_roi.getTopLeft().y);
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
			DEB_ERROR() << e2.GetDescription();
			throw LIMA_HW_EXC(Error,e2.GetDescription());
		}
		DEB_ERROR() << e.GetDescription();		
        throw LIMA_HW_EXC(Error, e.GetDescription());
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
				static_cast<int>(Camera_->Width())	,
				static_cast<int>(Camera_->Height())
			  );
		hw_roi = r;

	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << e.GetDescription();
        throw LIMA_HW_EXC(Error, e.GetDescription());
    }	
	DEB_RETURN() << DEB_VAR1(hw_roi);
}


//-----------------------------------------------------
//
//-----------------------------------------------------
#ifndef LESSDEPENDENCY
void Camera::handle_message( yat::Message& msg )  throw( yat::Exception )
{
  DEB_MEMBER_FUNCT();  
  try
  {
    switch ( msg.type() )
    {
      //-----------------------------------------------------	
      case yat::TASK_INIT:
      {
        DEB_TRACE() <<"Camera::->TASK_INIT";
      }
      break;
      //-----------------------------------------------------    
      case yat::TASK_EXIT:
      {
        DEB_TRACE() <<"Camera::->TASK_EXIT";
      }
      break;
      //-----------------------------------------------------    
      case yat::TASK_TIMEOUT:
      {
		DEB_TRACE() <<"Camera::->TASK_TIMEOUT";
      }
      break;
      //-----------------------------------------------------    
#else
void Camera::execCmd(int cmd)
{
  DEB_MEMBER_FUNCT();
  switch(cmd)
    {
#endif
      case BASLER_START_MSG:	
      {
		DEB_TRACE() << "Camera::->BASLER_START_MSG";
		m_stop_already_done = false;
#ifndef LESSDEPENDENCY
		this->post(new yat::Message(BASLER_GET_IMAGE_MSG), kPOST_MSG_TMO);
#else
		sendCmd(BASLER_GET_IMAGE_MSG);
#endif
      }
      break;
      //-----------------------------------------------------
      case BASLER_GET_IMAGE_MSG:
      {
		DEB_TRACE() << "Camera::->BASLER_GET_IMAGE_MSG";
		GetImage();
      }
      break;	
      //-----------------------------------------------------
      case BASLER_STOP_MSG:
      {
		DEB_TRACE() << "Camera::->BASLER_STOP_MSG";
		FreeImage();
      }
      break;
      //-----------------------------------------------------
    }
#ifndef LESSDEPENDENCY
}
  catch( yat::Exception& ex )
  {
    DEB_ERROR() <<"Error : " << ex.errors[0].desc;
    throw;
  }
#endif
}

//-----------------------------------------------------
// Constructor allocates the image buffer
//-----------------------------------------------------
CGrabBuffer::CGrabBuffer(size_t ImageSize)
{
    m_pBuffer = new uint8_t[ ImageSize ];
    if (NULL == m_pBuffer)
    {
        GenICam::GenericException e("Not enough memory to allocate image buffer", __FILE__, __LINE__);
        throw e;
    }
}

//-----------------------------------------------------
// Freeing the memory
//-----------------------------------------------------
CGrabBuffer::~CGrabBuffer()
{
    if (NULL != m_pBuffer)
        delete[] m_pBuffer;
}
//-----------------------------------------------------

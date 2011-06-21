#include "BaslerCamera.h"
#include <sstream>
#include <iostream>
#include <string>
#include <math.h>
using namespace lima;
using namespace lima::Basler;
using namespace std;

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

class Camera::_AcqThread : public Thread
{
  DEB_CLASS_NAMESPC(DebModCamera, "Camera", "_AcqThread");
public:
  _AcqThread(Camera &aCam) : m_cam(aCam) {}
  virtual ~_AcqThread();

protected:
  virtual void threadFunction();
  
private:
  Camera&	m_cam;
};
//---------------------------
//- Ctor
//---------------------------
Camera::Camera(const std::string& camera_ip,int packet_size)
		: m_buffer_cb_mgr(m_buffer_alloc_mgr),
		  m_buffer_ctrl_mgr(m_buffer_cb_mgr),
		  m_nb_frames(1),
		  m_status(Ready),
		  m_wait_flag(true),
		  m_quit(false),
		  m_thread_running(true),
		  m_image_number(0),
		  m_exp_time(1.),
		  pTl_(NULL),
		  Camera_(NULL),
		  StreamGrabber_(NULL)
{
	DEB_CONSTRUCTOR();
	m_camera_ip = camera_ip;
	try
    {		
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
	Pylon::String_t pylon_camera_ip(_get_ip_addresse(m_camera_ip.c_str()));
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

	
	if(packet_size > 0)
	  Camera_->GevSCPSPacketSize.SetValue(packet_size);

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
	Roi aFullFrame(0,0,Camera_->WidthMax(),Camera_->HeightMax());
	setRoi(aFullFrame);
	// Set Binning to 1
	Camera_->BinningVertical.SetValue(1);
	Camera_->BinningHorizontal.SetValue(1);

        // Set the camera to continuous frame mode
	DEB_TRACE() << "Set the camera to continuous frame mode";
        Camera_->TriggerSelector.SetValue(TriggerSelector_AcquisitionStart);
        Camera_->AcquisitionMode.SetValue(AcquisitionMode_Continuous);
	Camera_->ExposureAuto.SetValue(ExposureAuto_Off);

        // Get the image buffer size
	DEB_TRACE() << "Get the image buffer size";
	ImageSize_ = (size_t)(Camera_->PayloadSize.GetValue());
       


	m_acq_thread = new _AcqThread(*this);
	m_acq_thread->start();
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
		// Stop Acq thread
		delete m_acq_thread;
		// Close stream grabber
		DEB_TRACE() << "Close stream grabber";
		delete StreamGrabber_;
		// Close camera
		DEB_TRACE() << "Close camera";
		delete Camera_;
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << e.GetDescription();
		Pylon::PylonTerminate( );
        throw LIMA_HW_EXC(Error, e.GetDescription());
    }
	delete pTl_;
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
		// Get the first stream grabber object of the selected camera
		DEB_TRACE() << "Get the first stream grabber object of the selected camera";
		StreamGrabber_ = new Camera_t::StreamGrabber_t(Camera_->GetStreamGrabber(0));
		// Open the stream grabber
		DEB_TRACE() << "Open the stream grabber";
		StreamGrabber_->Open();
		if(!StreamGrabber_->IsOpen())
		  {
		    delete StreamGrabber_;
		    StreamGrabber_ = NULL;
		    DEB_ERROR() <<"Unable to open the steam grabber!";
		    throw LIMA_HW_EXC(Error,"Unable to open the steam grabber!");
		  }
		// We won't use image buffers greater than ImageSize
		DEB_TRACE() << "We won't use image buffers greater than ImageSize";
		StreamGrabber_->MaxBufferSize.SetValue((const size_t)ImageSize_);

		StdBufferCbMgr& buffer_mgr = m_buffer_cb_mgr;
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
		    int buffer_nb,concat_frame_nb;
		    buffer_mgr.acqFrameNb2BufferNb(i,buffer_nb,concat_frame_nb);
		    void *ptr = buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb);
		    // The registration returns a handle to be used for queuing the buffer.
		    DEB_TRACE() << "RegisterBuffer" << DEB_VAR1(ptr);
		    StreamBufferHandle bufferId = StreamGrabber_->RegisterBuffer(ptr,(const size_t)ImageSize_);
		    DEB_TRACE() << "Queue buffer" << DEB_VAR1(bufferId);
		    StreamGrabber_->QueueBuffer(bufferId, NULL);
		  }
		
		// Let the camera acquire images continuously ( Acquisiton mode equals Continuous! )
		DEB_TRACE() << "Let the camera acquire images continuously";
		// Wait running stat of acquisition thread
		AutoMutex aLock(m_cond.mutex());
		m_wait_flag = false;
		m_cond.broadcast();
		while(!m_thread_running)
		  m_cond.wait();

		buffer_mgr.setStartTimestamp(Timestamp::now());
		Camera_->AcquisitionStart.Execute();

		
	}
	catch (GenICam::GenericException &e)
    {
        // Error handling
		DEB_ERROR() << e.GetDescription();
        throw LIMA_HW_EXC(Error, e.GetDescription());
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
			m_wait_flag = true;
			while(!internalFlag && m_thread_running)
			  m_cond.wait();
			aLock.unlock();

			//Let the acq thread stop the acquisition
			if(!internalFlag) return;
			
			// Stop acquisition
			DEB_TRACE() << "Stop acquisition";
			Camera_->AcquisitionStop.Execute();
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
			_setStatus(Camera::Ready,false);
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
//- Camera::_AcqThread::threadFunction()
//---------------------------
void Camera::_AcqThread::threadFunction()
{
  DEB_MEMBER_FUNCT();
  AutoMutex aLock(m_cam.m_cond.mutex());
  StdBufferCbMgr& buffer_mgr = m_cam.m_buffer_cb_mgr;

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
	bool continueAcq = true;
	while(!m_cam.m_wait_flag && 
	      continueAcq &&(!m_cam.m_nb_frames || m_cam.m_image_number < m_cam.m_nb_frames))
	  {
	    int local_exp_time = int(m_cam.m_exp_time + 1.) * 1000;
	    if (m_cam.StreamGrabber_->GetWaitObject().Wait(local_exp_time))
	      {
		// Get the grab result from the grabber's result queue
		GrabResult Result;
		m_cam.StreamGrabber_->RetrieveResult(Result);
		if (Grabbed == Result.Status())
		  {
		    // Grabbing was successful, process image
		    m_cam._setStatus(Camera::Readout,false);
		    DEB_TRACE()  << "image#" << DEB_VAR1(m_cam.m_image_number) <<" acquired !";
		    int nb_buffers;
		    buffer_mgr.getNbBuffers(nb_buffers);
		    if (!m_cam.m_nb_frames || m_cam.m_image_number < int(m_cam.m_nb_frames - nb_buffers))
		      m_cam.StreamGrabber_->QueueBuffer(Result.Handle(),NULL);

				
		    HwFrameInfoType frame_info;
		    frame_info.acq_frame_nb = m_cam.m_image_number;
		    continueAcq = buffer_mgr.newFrameReady(frame_info);
		    DEB_TRACE() << DEB_VAR1(continueAcq);
		    ++m_cam.m_image_number;
		  }
		else if (Failed == Result.Status())
		  {
		    // Error handling
		    DEB_ERROR() << "No image acquired!"
				<< " Error code : 0x" << DEB_VAR1(hex) << " " << Result.GetErrorCode()
				<< " Error description : " << Result.GetErrorDescription();
				
		    if(!m_cam.m_nb_frames) //Do not stop acquisition in "live" mode, just IGNORE  error
		      m_cam.StreamGrabber_->QueueBuffer(Result.Handle(), NULL);
		    else			//in "snap" mode , acquisition must be stopped
		      {
			m_cam._setStatus(Camera::Fault,false);
			continueAcq = false;
		      }
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
	m_cam.m_wait_flag = true;
      }			
    }
}
//-----------------------------------------------------
//
//-----------------------------------------------------

Camera::_AcqThread::~_AcqThread()
{
  AutoMutex aLock(m_cam.m_cond.mutex());
  m_cam.m_quit = true;
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
        
        if (m_detector_model.compare(0,3,"acA")!=0)
	  {
            //Not a ACE model, if scout or pilot, exposure time has to be adjusted using
            // the exposure time base + the exposure time raw.
            //see ImageGrabber for more details !!!
            Camera_->ExposureTimeBaseAbs.SetValue(100.0); //- to be sure we can set the Raw setting on the full range (1 .. 4095)
            double raw = ::ceil( exp_time / 50 );
            Camera_->ExposureTimeRaw.SetValue(static_cast<int>(raw));
            raw = static_cast<double>(Camera_->ExposureTimeRaw.GetValue());      
            Camera_->ExposureTimeBaseAbs.SetValue(1E6 * exp_time / Camera_->ExposureTimeRaw.GetValue());	
	  } else {        
	  // this is a ACE camera model, it supports direct programming of the exposure using
	  // the exposure time absolute.
	  Camera_->ExposureTimeAbs.SetValue(1E6 * exp_time );	
        }
	  
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
      lat_time = 0;
      DEB_RETURN() << DEB_VAR1(lat_time);
  }

  //-----------------------------------------------------
  //
  //-----------------------------------------------------
  void Camera::getExposureTimeRange(double& min_expo, double& max_expo) const
  {
    DEB_MEMBER_FUNCT();
    
    if (m_detector_model.compare(0,3,"acA")!=0)
      {
        min_expo = 1e-6;
        max_expo = 1e9;
      }else 
      {
        min_expo = Camera_->ExposureTimeAbs.GetMin()*1e-6;
        max_expo = Camera_->ExposureTimeAbs.GetMax()*1e-6;
      }

    DEB_RETURN() << DEB_VAR2(min_expo, max_expo);
  }

  //-----------------------------------------------------
  //
  //-----------------------------------------------------
  void Camera::getLatTimeRange(double& min_lat, double& max_lat) const
  {   
    DEB_MEMBER_FUNCT();
    /////@@@@ TODO if necessary
      min_lat= 0;
      max_lat= 0;
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
    hw_roi = set_roi;
	
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
	if(r == set_roi) return;
	//- first reset the ROI
	Camera_->OffsetX.SetValue(Camera_->OffsetX.GetMin());
	Camera_->OffsetY.SetValue(Camera_->OffsetY.GetMin());
	Camera_->Width.SetValue(Camera_->Width.GetMax());
	Camera_->Height.SetValue(Camera_->Height.GetMax());

	if(set_roi.isActive())
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

//###########################################################################
// This file is part of LImA, a Library for Image Acquisition
//
// Copyright (C) : 2009-2022
// European Synchrotron Radiation Facility
// CS40220 38043 Grenoble Cedex 9 
// FRANCE
//
// Contact: lima@esrf.fr
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
//- EventHandler
//---------------------------
class Camera::_EventHandler : public CBaslerUniversalCameraEventHandler,
			      public CBaslerUniversalImageEventHandler
{
  DEB_CLASS_NAMESPC(DebModCamera, "Camera", "_EventHandler");
public:
  _EventHandler(Camera &aCam) :
    m_cam(aCam), m_buffer_mgr(m_cam.m_buffer_ctrl_obj.getBuffer())
  {
  };

  virtual void 	OnImageGrabbed(CBaslerUniversalInstantCamera &camera,
			       const CBaslerUniversalGrabResultPtr &grabResult);
  
  unsigned short	m_block_id;
private:
  void _check_missing_frame(const CBaslerUniversalGrabResultPtr &ptrGrabResult);
  
  Camera&		m_cam;
  StdBufferCbMgr&	m_buffer_mgr;
};


//---------------------------
//- Ctor
//---------------------------
Camera::Camera(const std::string& camera_id,int packet_size,int receive_priority)
        : m_nb_frames(1),
          m_status(Ready),
	  m_image_number(0),
          m_exp_time(1.),
          m_latency_time(0.),
          m_socketBufferSize(0),
          m_is_usb(false),
	  m_blank_image_for_missed(false),
          Camera_(NULL),
	  m_event_handler(NULL),
          m_receive_priority(receive_priority),
	  m_video_flag_mode(false),
	  m_video(NULL)
{
    DEB_CONSTRUCTOR();
    m_camera_id = camera_id;
    try
    {
        // Create the transport layer object needed to enumerate or
        // create a camera object of type Camera_t::DeviceClass()
        DEB_TRACE() << "Create a camera object of type Camera_t::DeviceClass()";
        CTlFactory& TlFactory = CTlFactory::GetInstance();

        CDeviceInfo di;

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
        IPylonDevice* device = CTlFactory::GetInstance().CreateFirstDevice(di);
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
        m_is_usb = Camera_->GetDeviceInfo().IsUsbDriverTypeAvailable();

        //- Infos:
        DEB_TRACE() << DEB_VAR2(m_detector_type,m_detector_model);
        DEB_TRACE() << "SerialNumber    = " << Camera_->GetDeviceInfo().GetSerialNumber();
        DEB_TRACE() << "UserDefinedName = " << Camera_->GetDeviceInfo().GetUserDefinedName();
        DEB_TRACE() << "DeviceVersion   = " << Camera_->GetDeviceInfo().GetDeviceVersion();
        DEB_TRACE() << "DeviceFactory   = " << Camera_->GetDeviceInfo().GetDeviceFactory();
        DEB_TRACE() << "FriendlyName    = " << Camera_->GetDeviceInfo().GetFriendlyName();
        DEB_TRACE() << "FullName        = " << Camera_->GetDeviceInfo().GetFullName();
        DEB_TRACE() << "DeviceClass     = " << Camera_->GetDeviceInfo().GetDeviceClass();

	// Register Event handler
        m_event_handler = new _EventHandler(*this);
	Camera_->RegisterImageEventHandler(m_event_handler,
					   RegistrationMode_ReplaceAll,
					   Cleanup_None);
	// Camera event processing must be enabled first. The default is off.
	Camera_->GrabCameraEvents = true;
        // Open the camera
        DEB_TRACE() << "Open camera";
        Camera_->Open();

	if(!Camera_->EventSelector.IsWritable())
	  THROW_HW_ERROR(Error) << "The device doesn't support events.";
	
        if(packet_size > 0 && !m_is_usb) {
          Camera_->GevSCPSPacketSize.SetValue(packet_size);

        }
    
        // Set the image format and AOI
        DEB_TRACE() << "Set the image format and AOI";
	// basler model string last character codes for color (c) or monochrome (m)
	std::list<string> formatList;

	if (m_detector_model.find("gc") != std::string::npos ||
	    m_detector_model.find("uc") != std::string::npos
	    )
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
        Camera_->AcquisitionMode.SetValue(AcquisitionMode_Continuous);
        if ( IsAvailable(Camera_->ExposureAuto ))
        {
            DEB_TRACE() << "Set ExposureAuto to Off";           
            Camera_->ExposureAuto.SetValue(ExposureAuto_Off);
        }

	if (IsAvailable(Camera_->TestImageSelector ))
	{
            DEB_TRACE() << "Set TestImage to Off";           
            Camera_->TestImageSelector.SetValue(TestImageSelector_Off);	  
	}
	// Start with internal trigger
	// Force cache variable (camera register) to get trigger really initialized at first call
	m_trigger_mode = ExtTrigSingle;
	setTrigMode(IntTrig);
	// Same thing for exposure time, camera stays with previous setting
	double min_exp, max_exp;
	getExposureTimeRange(min_exp, max_exp);
	//fast basler models do not support 1.0 second exposure but lower value
	double exp_time = min(1.0, max_exp);
	setExpTime(exp_time);
        // Get the image buffer size
        DEB_TRACE() << "Get the image buffer size";
        ImageSize_ = (size_t)(Camera_->PayloadSize.GetValue());
    }
    catch (Pylon::GenericException &e)
    {
      DeviceInfoList_t list;
      CTlFactory::GetInstance().EnumerateDevices(list);
      if(!list.empty())
	DEB_ALWAYS() << "Device founds:";
      else
	DEB_ALWAYS() << "No Camera found!";
      for(auto dev: list)
	{
	  DEB_ALWAYS() << "------------------------------------";
	  DEB_ALWAYS() << "SerialNumber    = " << dev.GetSerialNumber();
	  DEB_ALWAYS() << "UserDefinedName = " << dev.GetUserDefinedName();
	  DEB_ALWAYS() << "DeviceVersion   = " << dev.GetDeviceVersion();
	  DEB_ALWAYS() << "DeviceFactory   = " << dev.GetDeviceFactory();
	  DEB_ALWAYS() << "FriendlyName    = " << dev.GetFriendlyName();
	  DEB_ALWAYS() << "FullName        = " << dev.GetFullName();
	  DEB_ALWAYS() << "DeviceClass     = " << dev.GetDeviceClass();
	  DEB_ALWAYS() << "\n";
	}
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }

    // if color camera video capability will be available
    m_video_flag_mode = m_color_flag;
}

//---------------------------
//- Dtor
//---------------------------
Camera::~Camera()
{
    DEB_DESTRUCTOR();
    try
    {
        Camera_->DeregisterImageEventHandler(m_event_handler);
        // Stop Acq thread
        delete m_event_handler;
        m_event_handler = NULL;
        
        // Close camera
        DEB_TRACE() << "Close camera";
        delete Camera_;
        Camera_ = NULL;
    }
    catch (Pylon::GenericException &e)
    {
        // Error handling
        DEB_ERROR() << e.GetDescription();
    }
}

void Camera::prepareAcq()
{
    DEB_MEMBER_FUNCT();
    m_image_number=0;
    // new flag to better manage multiple acqStart() with trigger mode IntTrigMult
    // startAcq can be recalled before the threadFunction has processed the new image and
    // incremented the counter m_image_number
    m_acq_started = false;
    m_event_handler->m_block_id = 0; // reset block id counter
}

//---------------------------
//- Camera::start()
//---------------------------
void Camera::startAcq()
{
    DEB_MEMBER_FUNCT();
    try
    {
	_startAcq();

	// start acquisition at first image
	// code moved from prepareAcq(), otherwise with color camera
	// CtVideo::_prepareAcq() which calls stopAcq() will kill the acquisition 
	if(m_trigger_mode == IntTrigMult)
	  this->Camera_->TriggerSoftware.Execute();
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
  if(!m_acq_started)
    {
      if(m_video)
	m_video->getBuffer().setStartTimestamp(Timestamp::now());
      else
	m_buffer_ctrl_obj.getBuffer().setStartTimestamp(Timestamp::now());

      if (m_nb_frames)
	Camera_->StartGrabbing(m_nb_frames,GrabStrategy_OneByOne,GrabLoop_ProvidedByInstantCamera);
      else
	Camera_->StartGrabbing(GrabStrategy_OneByOne,GrabLoop_ProvidedByInstantCamera);
      m_acq_started = true;
    }
  
  try
    {
      if(m_trigger_mode != IntTrig)
	Camera_->WaitForFrameTriggerReady(1000, TimeoutHandling_ThrowException);
    }
  catch(GenICam::GenericException &e)
    {
      THROW_HW_ERROR(Error) << "Wait ready for trigger failed: "
			    << e.GetDescription();
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
      // Stop acquisition
      DEB_TRACE() << "Stop acquisition";
      Camera_->StopGrabbing();
      _setStatus(Camera::Ready,false);
    }
    catch (Pylon::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }    
}


void Camera::_forceVideoMode(bool force)
{
  DEB_MEMBER_FUNCT();
  m_video_flag_mode = force;
  
}
//---------------------------
//- Camera::_EventHandler::OnImageGrabbed()
//---------------------------
void Camera::_EventHandler::OnImageGrabbed(CBaslerUniversalInstantCamera &camera,
					   const CBaslerUniversalGrabResultPtr &ptrGrabResult)
{
  DEB_MEMBER_FUNCT();
  try
    {
      if(m_cam.m_video_flag_mode)
	{
	  VideoMode mode;
	  m_cam.m_video->getVideoMode(mode);
	  if(ptrGrabResult->GrabSucceeded())
	    {
	      m_cam.m_video->callNewImage((char*)ptrGrabResult->GetBuffer(),
					  ptrGrabResult->GetWidth(),
					  ptrGrabResult->GetHeight(),
					  mode);
	      ++m_cam.m_image_number;
	    }
      
	}
      else
	{
	  if (ptrGrabResult->GrabSucceeded())
	    {
	      // Access the image data.
	      uint8_t* pImageBuffer = (uint8_t*) ptrGrabResult->GetBuffer();

	      _check_missing_frame(ptrGrabResult);

	      HwFrameInfoType frame_info;
	      frame_info.acq_frame_nb = m_cam.m_image_number;
	      void *framePt = m_buffer_mgr.getFrameBufferPtr(m_cam.m_image_number);
	      const FrameDim& fDim = m_buffer_mgr.getFrameDim();
	      void* srcPt = ((char*)pImageBuffer);
	      DEB_TRACE() << "memcpy:" << DEB_VAR2(srcPt,framePt);
	      memcpy(framePt,srcPt,fDim.getMemSize());

	      if(!m_buffer_mgr.newFrameReady(frame_info))
		m_cam._stopAcq(true);

	      ++m_cam.m_image_number;
	    }
	}
    }
  catch (Pylon::GenericException &e)
    {
      // Error handling
      DEB_ERROR() << "GeniCam Error! "<< e.GetDescription();
      m_cam._setStatus(Camera::Fault, true);
    }
}

void Camera::_EventHandler::_check_missing_frame(const CBaslerUniversalGrabResultPtr &ptrGrabResult)
{
  DEB_MEMBER_FUNCT();
  
  if(!m_cam.m_is_usb) // GigE Camera
    {
      auto block_id = ptrGrabResult->GetBlockID();
      if(block_id)	// 0 -> not available for this camera
	{
	  ++m_block_id;
	  if(!m_block_id) ++m_block_id; // overflow to 0
	  if(block_id != m_block_id) // missed a frame
	    {
	      DEB_WARNING() << "Missed frame expected : "
			    << m_block_id
			    << " get : "
			    << block_id;
	      if(m_cam.m_blank_image_for_missed)
		{
		  unsigned short missed_frames = block_id - m_block_id;
		  if(m_block_id > block_id)  --missed_frames; // overflow
		  //missing frames are blank
		  for(int i = 0;i < missed_frames;++i)
		    {
		      void *framePt = m_buffer_mgr.getFrameBufferPtr(m_cam.m_image_number);
		      const FrameDim& fDim = m_buffer_mgr.getFrameDim();
		      memset(framePt,0,fDim.getMemSize());
		      HwFrameInfoType frame_info;
		      frame_info.acq_frame_nb = m_cam.m_image_number;
		      DEB_WARNING() << "Frame " << m_cam.m_image_number << " is blank";
		      if(!m_buffer_mgr.newFrameReady(frame_info))
			{
			  m_cam._stopAcq(true);
			  break;
			}
		      ++m_cam.m_image_number;
		    }
		}
	      m_block_id = block_id;
	    }
	}
    }
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
	    // setExposure() can disable FrameRate if latency_time is ~0,
	    // do not reenable FrameRate here if not required
	    // and when cold start the camera can have the FrameRate enabled
	    // from previous acquisition, so disable it if latency is 0
	    if (m_latency_time >= 1e-6)
	      this->Camera_->AcquisitionFrameRateEnable.SetValue(true);
	    else
	      this->Camera_->AcquisitionFrameRateEnable.SetValue(false);	    
	}
        else if ( mode == IntTrigMult )
        {
	    this->Camera_->TriggerMode.SetValue(TriggerMode_On);
	    this->Camera_->TriggerSource.SetValue(TriggerSource_Software);
            this->Camera_->AcquisitionFrameRateEnable.SetValue( false );
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
        TriggerActivationEnums act =
            static_cast<TriggerActivationEnums>(activation);

        // If the parameter TriggerActivation is available for this camera
        if (GenApi::IsAvailable(Camera_->TriggerActivation))
            Camera_->TriggerActivation.SetValue(act);
    }
    catch (Pylon::GenericException &e)
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
        TriggerActivationEnums act;

        // If the parameter AcquisitionFrameCount is available for this camera
        if (GenApi::IsAvailable(Camera_->TriggerActivation))
            act = Camera_->TriggerActivation.GetValue();

        activation = static_cast<TrigActivation>(act);

    }
    catch (Pylon::GenericException &e)
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
            // ExposureTimeBaseAbs is only available for GigE Ace camera
            if (IsAvailable(Camera_->ExposureTimeBaseAbs))
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
	      if (IsAvailable(Camera_->ExposureTime) || m_is_usb)
                    Camera_->ExposureTime.SetValue(1E6 * exp_time);
                else
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
            double rate = 1/ (m_latency_time + m_exp_time);
            Camera_->AcquisitionFrameRateEnable.SetValue(true);
            DEB_TRACE() << DEB_VAR1(rate);
	    if (IsAvailable(Camera_->AcquisitionFrameRate) || m_is_usb)
	    {
	        double minrate = Camera_->AcquisitionFrameRate.GetMin();
	        double maxrate = Camera_->AcquisitionFrameRate.GetMax();
		if (rate < minrate) rate = minrate;
		if (rate > maxrate) rate = maxrate;
	        Camera_->AcquisitionFrameRate.SetValue(rate);
		DEB_TRACE() << DEB_VAR1(Camera_->AcquisitionFrameRate.GetValue());
	    }
            else
	    {
	        double minrate = Camera_->AcquisitionFrameRateAbs.GetMin();
	        double maxrate = Camera_->AcquisitionFrameRateAbs.GetMax();
		if (rate < minrate) rate = minrate;
		if (rate > maxrate) rate = maxrate;
	        Camera_->AcquisitionFrameRateAbs.SetValue(rate);
		DEB_TRACE() << DEB_VAR1(Camera_->AcquisitionFrameRateAbs.GetValue());
	    }            
        }

    }
    catch (Pylon::GenericException &e)
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
        double value = 1.0E-6 * static_cast<double>(Camera_->ExposureTime.GetValue());    
        exp_time = value;
    }
    catch (Pylon::GenericException &e)
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
        // ExposureTimeBaseAbs is available fot GigE cams only
        if (IsAvailable(Camera_->ExposureTimeBaseAbs) && !m_is_usb)
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
	  if (IsAvailable(Camera_->ExposureTime) || m_is_usb) {
                min_expo = Camera_->ExposureTime.GetMin()*1e-6;
                max_expo = Camera_->ExposureTime.GetMax()*1e-6;
            } else {
                min_expo = Camera_->ExposureTimeAbs.GetMin()*1e-6;
                max_expo = Camera_->ExposureTimeAbs.GetMax()*1e-6;
            }
            
        }
    }
    catch (Pylon::GenericException &e)
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
        double minAcqFrameRate = 0;

        if (IsAvailable(Camera_->AcquisitionFrameRate) || m_is_usb)
            minAcqFrameRate = Camera_->AcquisitionFrameRate.GetMin();
        else
            minAcqFrameRate = Camera_->AcquisitionFrameRateAbs.GetMin();
        if (minAcqFrameRate > 0)
            max_lat = 1 / minAcqFrameRate;
        else
            max_lat = 0;
    }
    catch (Pylon::GenericException &e)
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
    aLock.unlock();
    if(status != Camera::Fault)
      status = Camera_->IsGrabbing() ? Camera::Exposure : Camera::Ready;
    //Check if camera is not waiting for trigger
    if((m_trigger_mode == IntTrigMult ||
	m_trigger_mode == ExtTrigMult ||
	m_trigger_mode == ExtGate) &&
       status == Camera::Exposure)
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
        if (GenApi::IsAvailable(Camera_->ResultingFrameRate) || m_is_usb)
            frame_rate = static_cast<double>(Camera_->ResultingFrameRate.GetValue());        
        else 
            frame_rate = static_cast<double>(Camera_->ResultingFrameRateAbs.GetValue());        
    }
    catch (Pylon::GenericException &e)
    {
        // Error handling
        THROW_HW_ERROR(Error) << e.GetDescription();
    }        
    DEB_RETURN() << DEB_VAR1(frame_rate);
}
//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setBlankImageForMissed(bool active)
{
  /** This will create blank images when missing an image.
   */
  m_blank_image_for_missed = active;
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
	    int x_inc =  Camera_->OffsetX.GetInc();
	    int y_inc =  Camera_->OffsetY.GetInc();
	    DEB_TRACE() << DEB_VAR2(x_inc, y_inc);

	    int x_left = int(set_roi.getTopLeft().x) / x_inc * x_inc;
	    int y_left = int(set_roi.getTopLeft().y) / y_inc * y_inc;
	    int x_right = int(set_roi.getTopLeft().x + set_roi.getSize().getWidth() + x_inc - 1) / x_inc * x_inc;
	    int y_right = int(set_roi.getTopLeft().y + set_roi.getSize().getHeight() + y_inc - 1) / y_inc * y_inc;
	    Roi rup_roi(x_left,y_left,
			x_right - x_left,
			y_right - y_left);

	    hw_roi = rup_roi;

	    DEB_TRACE() << DEB_VAR1(hw_roi);
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
        catch (Pylon::GenericException &e2)
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
      isAvailable = (IsAvailable(Camera_->OffsetX) && IsWritable(Camera_->OffsetX) &&
		     IsAvailable(Camera_->OffsetY) && IsWritable(Camera_->OffsetY) &&
		     IsAvailable(Camera_->Width) && IsWritable(Camera_->Width) &&
		     IsAvailable(Camera_->Height) && IsWritable(Camera_->Height));
    }
  catch(Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
	// new cameras like ACE2 have 2 temperatures one for coreboard and one for sensor
	// to change measurement DeviceTemperatureSelector should be call
	else if (IsAvailable(Camera_->DeviceTemperature))
	  temperature = Camera_->DeviceTemperature.GetValue();
    }
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
    int raw_gain, low_limit, high_limit;
    if (gain < 0. || gain > 1.)
      THROW_HW_ERROR(InvalidValue) << "Gain must be in range <0.0,1.0>";
    try
    {
        // you want to set the gain, remove autogain
        if (GenApi::IsAvailable(Camera_->GainAuto))
        {		
	    setAutoGain(false);
	}

	if (Camera_->GetSfncVersion() >= Sfnc_2_0_0) {
	    Camera_->GainSelector.SetValue(GainSelector_All);
	    
            low_limit = Camera_->Gain.GetMin();
            high_limit = Camera_->Gain.GetMax();
            raw_gain = int((high_limit - low_limit) * gain + low_limit);
            Camera_->Gain.SetValue(raw_gain);
        }
        else
        {
            Camera_->GainSelector.SetValue(GainSelector_All);
	    
            low_limit = Camera_->GainRaw.GetMin();
            high_limit = Camera_->GainRaw.GetMax();
            raw_gain = int((high_limit - low_limit) * gain + low_limit);
            Camera_->GainRaw.SetValue(raw_gain);
        }
	DEB_TRACE() << "low_limit   = " << low_limit;
	DEB_TRACE() << "high_limit = " << high_limit;
	DEB_TRACE() << "raw_gain    = " << raw_gain;
    }
    catch (Pylon::GenericException &e)
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
    int raw_gain, low_limit, high_limit;
    
    try
    {
        if (Camera_->GetSfncVersion() >= Sfnc_2_0_0) {
            raw_gain = Camera_->Gain.GetValue();
	    low_limit = Camera_->Gain.GetMin();
            high_limit = Camera_->Gain.GetMax();
        } else {
            raw_gain = Camera_->GainRaw.GetValue();
            low_limit = Camera_->GainRaw.GetMin();
	    high_limit = Camera_->GainRaw.GetMax();
        }
	DEB_TRACE() << "low_limit = " << low_limit;
	DEB_TRACE() << "high_limit = " << high_limit;
	DEB_TRACE() << "raw_gain    = " << raw_gain;
	gain = double(raw_gain - low_limit)/double(high_limit - low_limit);
    }
    catch (Pylon::GenericException &e)
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
        if (!m_is_usb)
            Camera_->GevSCFTD.SetValue(ftd);
    }
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
			THROW_HW_ERROR(Error)<<"AcquisitionFrameRateEnable Parameter is not Available !";
        }
    }
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
			THROW_HW_ERROR(Error)<<"AcquisitionFrameRateAbs Parameter is not Available !";
        }
    }
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
  if (!IsAvailable(Camera_->LineSelector) || !IsAvailable(Camera_->LineSource))
    {
       THROW_HW_ERROR(NotSupported) << "This camera model does not support LineSource and/or LineSelector";
    }
  
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
      THROW_HW_ERROR(NotSupported) << "Not yet supported";
    }
  Camera_->LineSource.SetValue(line_src);
}

void Camera::getOutput1LineSource(Camera::LineSource& source) const
{
  DEB_MEMBER_FUNCT();

  if (!IsAvailable(Camera_->LineSelector) || !IsAvailable(Camera_->LineSource))
    {
      source = Off;
      return;
    }
      
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
    catch (Pylon::GenericException &e)
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
    catch (Pylon::GenericException &e)
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
	count = Camera_->GetStreamGrabberParams().Statistic_Total_Buffer_Count.GetValue();
}

//---------------------------    
// The Failed Buffer Count will count only buffers, which were received with "status == failed". 
// That means, buffers that were incompletely grabbed (error code: 0xE1000014).
//---------------------------
void Camera::getStatisticsFailedBufferCount(long& count)
{
	DEB_MEMBER_FUNCT();
	count = Camera_->GetStreamGrabberParams().Statistic_Failed_Buffer_Count.GetValue();
}
//---------------------------    


//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setTestImageSelector(TestImageSelector sel)
{
    DEB_MEMBER_FUNCT();

    if (GenApi::IsAvailable(Camera_->TestImageSelector))
      {
	THROW_HW_ERROR(NotSupported) << "This camera model does not support TestImageSelector";
      }

    try
    {
        TestImageSelectorEnums test =
	  static_cast<TestImageSelectorEnums>(sel);
	Camera_->TestImageSelector.SetValue(test);
    }
    catch (Pylon::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getTestImageSelector(TestImageSelector& sel) const
{
    DEB_MEMBER_FUNCT();
    try
    {
        TestImageSelectorEnums test;

        // If the parameter TestImage is available for this camera
        if (GenApi::IsAvailable(Camera_->TestImageSelector))
	  {
            test = Camera_->TestImageSelector.GetValue();
	    sel = static_cast<TestImageSelector>(test);
	  }
	else
	  sel = TestImage_Off;

    }
    catch (Pylon::GenericException &e)
    {
        DEB_WARNING() << e.GetDescription();
    }
}

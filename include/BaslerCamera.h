#ifndef BASLERCAMERA_H
#define BASLERCAMERA_H

#ifndef LESSDEPENDENCY
///////////////////////////////////////////////////////////
// YAT::TASK 
///////////////////////////////////////////////////////////
#include <yat/threading/Task.h>
#include <yat/network/Address.h>

#define kLO_WATER_MARK      128
#define kHI_WATER_MARK      512

#define kPOST_MSG_TMO       2

const size_t  BASLER_START_MSG		=	(yat::FIRST_USER_MSG + 100);
const size_t  BASLER_STOP_MSG		=	(yat::FIRST_USER_MSG + 101);
const size_t  BASLER_GET_IMAGE_MSG	=	(yat::FIRST_USER_MSG + 102);
#endif

///////////////////////////////////////////////////////////


#include <pylon/PylonIncludes.h>
#include <pylon/gige/BaslerGigEDeviceInfo.h>
#include <stdlib.h>
#include <limits>
#include "HwMaxImageSizeCallback.h"
#include "HwBufferMgr.h"

using namespace Pylon;
using namespace std;

#if defined( USE_1394 )
// Settings to use  Basler 1394 cameras
#include <pylon/1394/Basler1394Camera.h>
typedef Pylon::CBasler1394Camera Camera_t;
using namespace Basler_IIDC1394CameraParams;
using namespace Basler_IIDC1394StreamParams;
#elif defined ( USE_GIGE )
// settings to use Basler GigE cameras
#include <pylon/gige/BaslerGigECamera.h>
typedef Pylon::CBaslerGigECamera Camera_t;
using namespace Basler_GigECameraParams;
using namespace Basler_GigEStreamParams;
#else
#error Camera type is not specified. For example, define USE_GIGE for using GigE cameras
#endif



namespace lima
{
namespace Basler
{
//----------------------------------------------------------------------------------------------------
class CGrabBuffer
{
    public:
        CGrabBuffer( size_t ImageSize);
        ~CGrabBuffer();
        uint8_t* GetBufferPointer(void) { return m_pBuffer; }
        StreamBufferHandle GetBufferHandle(void) { return m_hBuffer; }
        void SetBufferHandle(StreamBufferHandle hBuffer) { m_hBuffer = hBuffer; }

    protected:
        uint8_t *m_pBuffer;
        StreamBufferHandle m_hBuffer;
};


/*******************************************************************
 * \class Camera
 * \brief object controlling the basler camera via Pylon driver
 *******************************************************************/
#ifndef LESSDEPENDENCY		
class Camera : public yat::Task
#else
  class Camera : public CmdThread
#endif
{
	DEB_CLASS_NAMESPC(DebModCamera, "Camera", "Basler");

 public:

	enum Status {
	  Ready = CmdThread::MaxThreadStatus, Exposure, Readout, Latency, Fault
	};
#ifdef LESSDEPENDENCY
	enum { // Cmd
	  BASLER_START_MSG = MaxThreadCmd,BASLER_STOP_MSG,BASLER_GET_IMAGE_MSG
	};
#endif	
	Camera(const std::string& camera_ip);
	~Camera();

    void startAcq();
    void stopAcq();
    // -- detector info
    void getPixelSize(double& size);
    void getImageType(ImageType& type);
	void setImageType(ImageType type);

    void getDetectorType(std::string& type);
    void getDetectorModel(std::string& model);
	void getDetectorImageSize(Size& size);
	
	// -- Buffer control bject
	BufferCtrlMgr& getBufferMgr();
	
	//-- Synch control onj
	void setTrigMode(TrigMode  mode);
	void getTrigMode(TrigMode& mode);
	
	void setExpTime(double  exp_time);
	void getExpTime(double& exp_time);

	void setLatTime(double  lat_time);
	void getLatTime(double& lat_time);

	void setNbFrames(int  nb_frames);
	void getNbFrames(int& nb_frames);
	void getNbHwAcquiredFrames(int &nb_acq_frames);

	void checkRoi(const Roi& set_roi, Roi& hw_roi);
	void setRoi(const Roi& set_roi);
	
	void getRoi(Roi& hw_roi);	
	
	void getStatus(Camera::Status& status);
	
	// -- basler specific, LIMA don't worr'y about it !
	void getFrameRate(double& frame_rate);
	
#ifndef LESSDEPENDENCY
  //- [yat::Task implementation]
  protected: 
    virtual void handle_message( yat::Message& msg )      throw (yat::Exception);
#else
 protected:
    virtual void execCmd(int);
    virtual void init() {}
#endif
 private:
	void GetImage();
    void FreeImage();
	
	//- lima stuff
	SoftBufferAllocMgr 			m_buffer_alloc_mgr;
	StdBufferCbMgr 				m_buffer_cb_mgr;
	BufferCtrlMgr 				m_buffer_ctrl_mgr;
	int 						m_nb_frames;	
#ifndef LESSDEPENDENCY
	Camera::Status				m_status;
#endif
    int                         m_image_number;
    bool                        m_stop_already_done;
    double						m_exp_time;
	//- basler stuff 
	string						m_camera_ip;
	string 						m_detector_model;
	string 						m_detector_type;
	static const double 		PixelSize= 55.0;

#ifndef LESSDEPENDENCY    
    //- Mutex
	yat::Mutex 					lock_;
#endif
	//- Pylon stuff
	ITransportLayer* 			pTl_;
	DeviceInfoList_t 			devices_;
	Camera_t* 					Camera_;
	Camera_t::StreamGrabber_t* 	StreamGrabber_;
	std::vector<CGrabBuffer*> 	BufferList_;
	size_t 						ImageSize_;	
};
} // namespace Basler
} // namespace lima


#endif // BASLERCAMERA_H

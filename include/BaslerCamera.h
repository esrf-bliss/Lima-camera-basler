#ifndef BASLERCAMERA_H
#define BASLERCAMERA_H

#if defined (__GNUC__) && (__GNUC__ == 3) && defined (__ELF__)
#   define GENAPI_DECL __attribute__((visibility("default")))
#   define GENAPI_DECL_ABSTRACT __attribute__((visibility("default")))
#endif

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
/*******************************************************************
 * \class Camera
 * \brief object controlling the basler camera via Pylon driver
 *******************************************************************/
class Camera
{
	DEB_CLASS_NAMESPC(DebModCamera, "Camera", "Basler");
	friend class Interface;
 public:

	enum Status {
	  Ready, Exposure, Readout, Latency, Fault
	};
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

    void getExposureTimeRange(double& min_expo, double& max_expo) const;
    void getLatTimeRange(double& min_lat, double& max_lat) const;    

	void setNbFrames(int  nb_frames);
	void getNbFrames(int& nb_frames);
	void getNbHwAcquiredFrames(int &nb_acq_frames);

	void checkRoi(const Roi& set_roi, Roi& hw_roi);
	void setRoi(const Roi& set_roi);
	
	void getRoi(Roi& hw_roi);	
	
	void getStatus(Camera::Status& status);
	// -- basler specific, LIMA don't worr'y about it !
	void getFrameRate(double& frame_rate);
	
 private:
	class _AcqThread;
	friend class _AcqThread;
	void _stopAcq(bool);
	void _setStatus(Camera::Status status,bool force);

	//- lima stuff
	SoftBufferAllocMgr 		m_buffer_alloc_mgr;
	StdBufferCbMgr 			m_buffer_cb_mgr;
	BufferCtrlMgr 			m_buffer_ctrl_mgr;
	int 				m_nb_frames;	
	Camera::Status			m_status;
	volatile bool			m_wait_flag;
	volatile bool			m_quit;
	volatile bool			m_thread_running;
	int                         	m_image_number;
	double				m_exp_time;
	//- basler stuff 
	string				m_camera_ip;
	string 				m_detector_model;
	string 				m_detector_type;
	static const double 		PixelSize= 55.0;
	//- Pylon stuff
	ITransportLayer* 		pTl_;
	DeviceInfoList_t 		devices_;
	Camera_t* 			Camera_;
	Camera_t::StreamGrabber_t* 	StreamGrabber_;
	size_t 				ImageSize_;
	_AcqThread*			m_acq_thread;
	Cond				m_cond;
};
} // namespace Basler
} // namespace lima


#endif // BASLERCAMERA_H

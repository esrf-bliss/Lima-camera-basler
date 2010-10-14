#ifndef BASLERCAMERA_H
#define BASLERCAMERA_H

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
/*	
namespace Basler
{
*/
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


class Camera : public HwMaxImageSizeCallbackGen
{
	DEB_CLASS_NAMESPC(DebModCamera, "Camera", "Basler");

 public:
	enum Status {
		Ready, Exposure, Readout, Latency,
	};
	
	Camera();
	~Camera();

	void start();
	void stop();

    // -- detector info
    void getImageSize(Size& size);
    void getPixelSize(double& size);
    void getImageType(ImageType& type);

    void getDetectorType(std::string& type);
    void getDetectorModel(std::string& model);
	BufferCtrlMgr& getBufferMgr();
	
	void setTrigMode(TrigMode  mode);
	void getTrigMode(TrigMode& mode);
	
	void setExpTime(double  exp_time);
	void getExpTime(double& exp_time);

	void setLatTime(double  lat_time);
	void getLatTime(double& lat_time);

	void setNbFrames(int  nb_frames);
	void getNbFrames(int& nb_frames);
	
	void getStatus(Camera::Status& status);
	
    static const double PixelSize= 55.0;
	void getFrameRate(double& frame_rate);
	
	void checkFlip(Flip& flip);
	void setFlip(const Flip& flip);
	void getFlip(Flip& flip);	
  protected:
    virtual void setMaxImageSizeCallbackActive(bool cb_active);	

 private:
	void setFlipMode(int  flip_mode);
	void getFlipMode(int& flip_mode);	
	void GetImages(void);
	//- lima stuff
	SoftBufferAllocMgr 			m_buffer_alloc_mgr;
	StdBufferCbMgr 				m_buffer_cb_mgr;
	BufferCtrlMgr 				m_buffer_ctrl_mgr;
	bool 						m_mis_cb_act;
	bool 						m_stop_request;
	TrigMode 					m_trig_mode;
	int 						m_nb_frames;	
	Camera::Status				m_status;
	//- Pylon stuff
	ITransportLayer* 			pTl_;
	DeviceInfoList_t 			devices_;
	Camera_t* 					Camera_;
	Camera_t::StreamGrabber_t* 	StreamGrabber_;
	std::vector<CGrabBuffer*> 	BufferList_;
	size_t 						ImageSize_;
 
	
	
};
/*
} // namespace Basler
*/
} // namespace lima


#endif // BASLERCAMERA_H

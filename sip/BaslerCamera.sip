
namespace Basler
{
  class Camera
  {
%TypeHeaderCode
#include <BaslerCamera.h>
%End

  public:

    enum Status {
      Ready = MaxThreadStatus, Exposure, Readout, Latency,
    };

    Camera(const std::string& camera_ip);
    ~Camera();

    void startAcq();
    void stopAcq();

    // -- detector info
    void getPixelSize(double& size /Out/);
    void getImageType(ImageType& type /Out/);

    void getDetectorType(std::string& type /Out/);
    void getDetectorModel(std::string& model /Out/);
    void getDetectorImageSize(Size& size /Out/);
    BufferCtrlMgr& getBufferMgr();
	
    void setTrigMode(TrigMode  mode);
    void getTrigMode(TrigMode& mode /Out/);
	
    void setExpTime(double  exp_time);
    void getExpTime(double& exp_time /Out/);

    void setLatTime(double  lat_time);
    void getLatTime(double& lat_time /Out/);

    void getExposureTimeRange(double& min_expo /Out/, double& max_expo /Out/) const;
    void getLatTimeRange(double& min_lat /Out/, double& max_lat /Out/) const;

    void setNbFrames(int  nb_frames);
    void getNbFrames(int& nb_frames /Out/);
	
    void checkRoi(const Roi& set_roi, Roi& hw_roi /Out/);
    void setRoi(const Roi& set_roi);
    void getRoi(Roi& hw_roi /Out/);	
	
    void getStatus(Basler::Camera::Status& status /Out/);
	
    void getFrameRate(double& frame_rate /Out/);
  };

};


namespace Basler
{
  class Camera
  {
%TypeHeaderCode
#include <BaslerCamera.h>
%End

  public:

    enum Status {
      Ready, Exposure, Readout, Latency,
    };

    Camera(const std::string& camera_ip,int mtu_size = -1,int received_priority = 0);
    ~Camera();

    void prepareAcq();
    void startAcq();
    void stopAcq();

    // -- detector info
    void getImageType(ImageType& type /Out/);

    void getDetectorType(std::string& type /Out/);
    void getDetectorModel(std::string& model /Out/);
    void getDetectorImageSize(Size& size /Out/);
    HwBufferCtrlObj* getBufferCtrlObj();
	
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

    void checkBin(Bin& /In,Out/);
    void setBin(const Bin&);
    void getBin(Bin& /Out/);

    void setInterPacketDelay(int ipd);

    void setFrameTransmissionDelay(int ftd);

    void setAutoGain(bool auto_gain);
    void getAutoGain(bool& auto_gain /Out/) const;

    void setGain(double gain);
    void getGain(double& gain /Out/) const;

    void getStatus(Basler::Camera::Status& status /Out/);
	
    void getFrameRate(double& frame_rate /Out/);
    void setTimeout(int TO);
  };

};


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

    enum LineSource {
      Off, ExposureActive, FrameTriggerWait, LineTriggerWait,
      Timer1Active, Timer2Active, Timer3Active, Timer4Active, TimerActive,
      UserOutput1, UserOutput2, UserOutput3, UserOutput4, UserOutput,
      TriggerReady, SerialTx, AcquisitionTriggerWait, ShaftEncoderModuleOut, FrequencyConverter,
      PatternGenerator1, PatternGenerator2, PatternGenerator3, PatternGenerator4,
      AcquisitionTriggerReady,
    };

    enum TrigActivation {
        RisingEdge=Basler_GigECamera::TriggerActivation_RisingEdge,
        FallingEdge=Basler_GigECamera::TriggerActivation_FallingEdge,
        AnyEdge=Basler_GigECamera::TriggerActivation_AnyEdge,
        LevelHigh=Basler_GigECamera::TriggerActivation_LevelHigh,
        LevelLow=Basler_GigECamera::TriggerActivation_LevelLow
    };

    Camera(const std::string& camera_ip,int mtu_size = -1,int received_priority = 0);
    ~Camera();

    void prepareAcq();
    void startAcq();
    void stopAcq();

    // -- detector info
    void setImageType(ImageType type);
    void getImageType(ImageType& type /Out/);

    void getDetectorType(std::string& type /Out/);
    void getDetectorModel(std::string& model /Out/);
    void getDetectorImageSize(Size& size /Out/);
    HwBufferCtrlObj* getBufferCtrlObj();
	
    void setTrigMode(TrigMode  mode);
    void getTrigMode(TrigMode& mode /Out/);

    void setTrigActivation(TrigActivation activation);
    void getTrigActivation(TrigActivation& activation /Out/);

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

    void setSocketBufferSize(int sbs);

    void setFrameTransmissionDelay(int ftd);

    void getFrameRate(double& frame_rate /Out/) const;
    bool isBinningAvailable() const;
    bool isRoiAvailable() const;
    void setTimeout(int TO);
    void reset();

    void setAutoGain(bool auto_gain);
    void getAutoGain(bool& auto_gain /Out/) const;

    void setGain(double gain);
    void getGain(double& gain /Out/) const;

    void getStatus(Basler::Camera::Status& status /Out/);
	

    bool isTemperatureAvailable() const;
    void getTemperature(double& temperature /Out/);
    void isColor(bool& color_flag /Out/) const;
    void hasVideoCapability(bool& video_flag /Out/) const;

    // -- change output line source
    void setOutput1LineSource(Basler::Camera::LineSource);
    void getOutput1LineSource(Basler::Camera::LineSource&) const;

    // -- change acq frame count
    void setAcquisitionFrameCount(int AFC);
    void getAcquisitionFrameCount(int& AFC /Out/) const;

    // -- Pylon buffers statistics
    void getStatisticsTotalBufferCount(long& count);    
    void getStatisticsFailedBufferCount(long& count);
  };

};

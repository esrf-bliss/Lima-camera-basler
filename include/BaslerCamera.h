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

#ifndef BASLERCAMERA_H
#define BASLERCAMERA_H

#include <stdlib.h>
#include <limits>

#if defined (__GNUC__) && (__GNUC__ == 3) && defined (__ELF__)
#   define GENAPI_DECL __attribute__((visibility("default")))
#   define GENAPI_DECL_ABSTRACT __attribute__((visibility("default")))
#endif

#include <pylon/PylonIncludes.h>

#include <basler_export.h>

#include "lima/HwMaxImageSizeCallback.h"
#include "lima/HwBufferMgr.h"


using namespace Pylon;

#include <pylon/BaslerUniversalInstantCamera.h>
typedef CBaslerUniversalInstantCamera Camera_t;
using namespace Basler_UniversalCameraParams;
using namespace Basler_UniversalStreamParams;


namespace lima
{
namespace Basler
{
/*******************************************************************
 * \class Camera
 * \brief object controlling the basler camera via Pylon driver
 *******************************************************************/
class VideoCtrlObj;
class BASLER_EXPORT Camera
{
    DEB_CLASS_NAMESPC(DebModCamera, "Camera", "Basler");
    friend class Interface;
    friend class VideoCtrlObj;
    friend class SyncCtrlObj;
 public:

    enum Status {
      Ready, Exposure, Readout, Latency, Fault
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
        RisingEdge=TriggerActivation_RisingEdge,
        FallingEdge=TriggerActivation_FallingEdge,
        AnyEdge=TriggerActivation_AnyEdge,
        LevelHigh=TriggerActivation_LevelHigh,
        LevelLow=TriggerActivation_LevelLow
    };

    enum TestImageSelector {
      TestImage_Off=TestImageSelector_Off,
      TestImage_1=TestImageSelector_Testimage1,
      TestImage_2=TestImageSelector_Testimage2,
      TestImage_3=TestImageSelector_Testimage3,
      TestImage_4=TestImageSelector_Testimage4,
      TestImage_5=TestImageSelector_Testimage5,
      TestImage_6=TestImageSelector_Testimage6,
      TestImage_7=TestImageSelector_Testimage7,
    };
    
    Camera(const std::string& camera_id,int packet_size = -1,int received_priority = 0);
    ~Camera();

    void prepareAcq();
    void startAcq();
    void stopAcq();
    
    // -- detector info object
    void getImageType(ImageType& type);
    void setImageType(ImageType type);

    void getDetectorType(std::string& type);
    void getDetectorModel(std::string& model);
    void getDetectorImageSize(Size& size);
    
    // -- Buffer control object
    HwBufferCtrlObj* getBufferCtrlObj();
    
    //-- Synch control object
    void setTrigMode(TrigMode  mode);
    void getTrigMode(TrigMode& mode);

    void setTrigActivation(TrigActivation activation);
    void getTrigActivation(TrigActivation& activation) const;

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

    void checkBin(Bin&);
    void setBin(const Bin&);
    void getBin(Bin&);

    void getStatus(Camera::Status& status);

    // -- Transport Layer
    void setPacketSize(int isize);
    void getPacketSize(int& isize);    
    void setInterPacketDelay(int ipd);
    void getInterPacketDelay(int& ipd);
    void getMaxThroughput(int& ipd);    
    void getCurrentThroughput(int& ipd);
    void getBandwidthAssigned(int& ipd);

    void setSocketBufferSize(int sbs);
        
    void setFrameTransmissionDelay(int ftd);

    // -- basler specific, LIMA don't worry about it !
    void getFrameRate(double& frame_rate) const;
    bool isBinningAvailable() const;
    bool isRoiAvailable() const;
    void setTimeout(int TO);
    void reset();

    bool isGainAvailable() const;
    void setGain(double gain);
    void getGain(double& gain) const;

    bool isAutoGainAvailable() const;
    void setAutoGain(bool auto_gain);
    void getAutoGain(bool& auto_gain) const;

    bool isTemperatureAvailable() const;
    void getTemperature(double& temperature);    
    void isColor(bool& color_flag) const;
    void hasVideoCapability(bool& video_flag) const;

    // -- change output line source
    void setOutput1LineSource(LineSource src);
    void getOutput1LineSource(LineSource& src) const;

    // -- change acq frame count
    void setAcquisitionFrameCount(int AFC);
    void getAcquisitionFrameCount(int& AFC) const;

    // -- change AcquisitionFrameRateEnable
    void setAcquisitionFrameRateEnable(bool AFRE);
    void getAcquisitionFrameRateEnable(bool& AFRE) const;

    // -- change acq frame count
    void setAcquisitionFrameRateAbs(int AFRA);
    void getAcquisitionFrameRateAbs(int& AFRA) const;

    // -- Pylon buffers statistics
    void getStatisticsTotalBufferCount(long& count);    
    void getStatisticsFailedBufferCount(long& count);

    // -- Pylon test image selectors
    void setTestImageSelector(TestImageSelector sel);
    void getTestImageSelector(TestImageSelector& sel) const;
    
 private:
    class _EventHandler;
    friend class _EventHandler;
    void _stopAcq(bool);
    void _setStatus(Camera::Status status,bool force);
    void _startAcq();
    void _readTrigMode();
    void _forceVideoMode(bool force);

    //- lima stuff
    SoftBufferCtrlObj		m_buffer_ctrl_obj;
    int                         m_nb_frames;    
    Camera::Status              m_status;
    bool			m_acq_started;
    int                         m_image_number;
    double                      m_exp_time;
    int                         m_timeout;
    double                      m_latency_time;
    int                         m_socketBufferSize;
    bool                        m_is_usb;
    
    //- basler stuff 
    std::string                 m_camera_id;
    std::string                 m_detector_model;
    std::string                 m_detector_type;
    Size                        m_detector_size;
    
    //- Pylon stuff
    PylonAutoInitTerm             auto_init_term_;
    DeviceInfoList_t              devices_;
    Camera_t*                     Camera_;
    size_t                        ImageSize_;
    _EventHandler*                m_event_handler;
    Cond                          m_cond;
    int                           m_receive_priority;
    bool			  m_color_flag;
    bool			  m_video_flag_mode;
    VideoCtrlObj*		  m_video;
    TrigMode			  m_trigger_mode;
};
} // namespace Basler
} // namespace lima


#endif // BASLERCAMERA_H

//###########################################################################
// This file is part of LImA, a Library for Image Acquisition
//
// Copyright (C) : 2009-2011
// European Synchrotron Radiation Facility
// BP 220, Grenoble 38043
// FRANCE
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

    Camera(const std::string& camera_ip,int packet_size = -1,int received_priority = 0);
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

    void setInterPacketDelay(int ipd);

    void setFrameTransmissionDelay(int ftd);

    void getStatus(Camera::Status& status);
    // -- basler specific, LIMA don't worry about it !
    void getFrameRate(double& frame_rate);
    bool isBinnigAvailable(void);
    void setTimeout(int TO);
    void reset(void);

    void setGain(double gain);
    void getGain(double& gain) const;

    void setAutoGain(bool auto_gain);
    void getAutoGain(bool& auto_gain) const;

 private:
    class _AcqThread;
    friend class _AcqThread;
    void _stopAcq(bool);
    void _setStatus(Camera::Status status,bool force);
    void _freeStreamGrabber();

    //- lima stuff
    SoftBufferCtrlObj		m_buffer_ctrl_obj;
    int                         m_nb_frames;    
    Camera::Status              m_status;
    volatile bool               m_wait_flag;
    volatile bool               m_quit;
    volatile bool               m_thread_running;
    int                         m_image_number;
    double                      m_exp_time;
    int                         m_timeout;
    double                      m_latency_time;
    
    //- basler stuff 
    string                      m_camera_ip;
    string                      m_detector_model;
    string                      m_detector_type;
    
    //- Pylon stuff
    DeviceInfoList_t              devices_;
    Camera_t*                     Camera_;
    Camera_t::StreamGrabber_t*    StreamGrabber_;
    WaitObjectEx                  WaitObject_;
    size_t                        ImageSize_;
    _AcqThread*                   m_acq_thread;
    Cond                          m_cond;
    int                           m_receive_priority;
};
} // namespace Basler
} // namespace lima


#endif // BASLERCAMERA_H

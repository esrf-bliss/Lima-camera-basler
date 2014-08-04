#include "BaslerInterface.h"
#include "BaslerCamera.h"
#include "BaslerDetInfoCtrlObj.h"
#include "BaslerSyncCtrlObj.h"
#include "BaslerRoiCtrlObj.h"
#include "BaslerBinCtrlObj.h"
<<<<<<< HEAD
=======
#include "BaslerVideoCtrlObj.h"
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25

using namespace lima;
using namespace lima::Basler;


<<<<<<< HEAD
Interface::Interface(Camera *cam) :
  m_cam(cam)
{
	DEB_CONSTRUCTOR();
	m_det_info = new DetInfoCtrlObj(cam);
	m_sync = new SyncCtrlObj(cam);
	m_roi = new RoiCtrlObj(cam);
	m_bin = new BinCtrlObj(cam);
=======
Interface::Interface(Camera& cam) :
  m_cam(cam)
{
  DEB_CONSTRUCTOR();
  m_det_info = new DetInfoCtrlObj(cam);
  m_sync = new SyncCtrlObj(cam);
  m_roi = new RoiCtrlObj(cam);
  m_bin = new BinCtrlObj(cam);
  bool is_color_flag;
  m_cam.isColor(is_color_flag);
  if(is_color_flag)
    m_video = new VideoCtrlObj(cam);
  else
    m_video = NULL;
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

Interface::~Interface()
{
  DEB_DESTRUCTOR();
  delete m_det_info;
<<<<<<< HEAD
}

void Interface::getCapList(CapList &cap_list) const
{
	cap_list.push_back(HwCap(m_det_info));
	cap_list.push_back(HwCap(m_sync));
	cap_list.push_back(HwCap(m_roi));
	cap_list.push_back(HwCap(m_bin));
=======
  delete m_sync;
  delete m_roi;
  delete m_bin;
  delete m_video;
}

void Interface::getCapList(CapList &cap_list) const
{
  cap_list.push_back(HwCap(m_det_info));

  if(m_video)
    {
      cap_list.push_back(HwCap(m_video));
      cap_list.push_back(HwCap(&(m_video->getHwBufferCtrlObj())));
    }
  else
    {
      HwBufferCtrlObj* buffer = m_cam.getBufferCtrlObj();
      cap_list.push_back(HwCap(buffer));
    }

  cap_list.push_back(HwCap(m_sync));

  if(m_cam.isRoiAvailable())
    cap_list.push_back(HwCap(m_roi));

  if(m_cam.isBinningAvailable())
    cap_list.push_back(HwCap(m_bin));
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void Interface::reset(ResetLevel reset_level)
{
  DEB_MEMBER_FUNCT();
  DEB_PARAM() << DEB_VAR1(reset_level);

  stopAcq();

<<<<<<< HEAD
  m_cam->_setStatus(Camera::Ready,true);
=======
  m_cam._setStatus(Camera::Ready,true);
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void Interface::prepareAcq()
{
  DEB_MEMBER_FUNCT();
<<<<<<< HEAD
  m_cam->prepareAcq();
=======
  m_cam.prepareAcq();
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void Interface::startAcq()
{
<<<<<<< HEAD
	DEB_MEMBER_FUNCT();
	m_cam->startAcq();
=======
  DEB_MEMBER_FUNCT();
  m_cam.startAcq();
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void Interface::stopAcq()
{
  DEB_MEMBER_FUNCT();
<<<<<<< HEAD
  m_cam->stopAcq();
=======
  m_cam.stopAcq();
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void Interface::getStatus(StatusType& status)
{
<<<<<<< HEAD
	Camera::Status basler_status = Camera::Ready;
    m_cam->getStatus(basler_status);
    switch (basler_status)
    {
        case Camera::Ready:
            status.acq = AcqReady;
            status.det = DetIdle;
            break;
        case Camera::Exposure:
            status.det = DetExposure;
            status.acq = AcqRunning;
            break;
        case Camera::Readout:
            status.acq = AcqRunning;
            break;
        case Camera::Latency:
            status.det = DetLatency;
            status.acq = AcqRunning;
            break;
        case Camera::Fault:
          status.det = DetFault;
          status.acq = AcqFault;
=======
  Camera::Status basler_status = Camera::Ready;
  m_cam.getStatus(basler_status);
  switch (basler_status)
    {
    case Camera::Ready:
      status.set(HwInterface::StatusType::Ready);
      break;
    case Camera::Exposure:
      status.set(HwInterface::StatusType::Exposure);
      break;
    case Camera::Readout:
      status.set(HwInterface::StatusType::Readout);
      break;
    case Camera::Latency:
      status.set(HwInterface::StatusType::Latency);
      break;
    case Camera::Fault:
      status.set(HwInterface::StatusType::Fault);
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
    }
}

int Interface::getNbHwAcquiredFrames()
{
<<<<<<< HEAD
	DEB_MEMBER_FUNCT();
	int acq_frames;
	m_cam->getNbHwAcquiredFrames(acq_frames);
	return acq_frames;
}

void Interface::getFrameRate(double& frame_rate)
{
    DEB_MEMBER_FUNCT();
    m_cam->getFrameRate(frame_rate);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::setTimeout(int TO)
{
    m_cam->setTimeout(TO);
}
//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::setGain(double gain) { m_cam->setGain(gain); }
void Interface::getGain(double& gain) const { m_cam->getGain(gain); }

void Interface::setAutoGain(bool auto_gain) { m_cam->setAutoGain(auto_gain); }
void Interface::getAutoGain(bool& auto_gain) const { m_cam->getAutoGain(auto_gain); }
=======
  DEB_MEMBER_FUNCT();
  int acq_frames;
  m_cam.getNbHwAcquiredFrames(acq_frames);
  return acq_frames;
}

//-----------------------------------------------------
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25

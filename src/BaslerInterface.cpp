#include "BaslerInterface.h"
#include "BaslerCamera.h"
#include "BaslerDetInfoCtrlObj.h"
#include "BaslerSyncCtrlObj.h"
#include "BaslerRoiCtrlObj.h"
#include "BaslerBinCtrlObj.h"

using namespace lima;
using namespace lima::Basler;


Interface::Interface(Camera *cam) :
  m_cam(cam)
{
	DEB_CONSTRUCTOR();
	m_det_info = new DetInfoCtrlObj(cam);
	m_sync = new SyncCtrlObj(cam);
	m_roi = new RoiCtrlObj(cam);
	m_bin = new BinCtrlObj(cam);
}

Interface::~Interface()
{
  DEB_DESTRUCTOR();
  delete m_det_info;
}

void Interface::getCapList(CapList &cap_list) const
{
	cap_list.push_back(HwCap(m_det_info));
	cap_list.push_back(HwCap(m_sync));
	cap_list.push_back(HwCap(m_roi));
	cap_list.push_back(HwCap(m_bin));
}

void Interface::reset(ResetLevel reset_level)
{
  DEB_MEMBER_FUNCT();
  DEB_PARAM() << DEB_VAR1(reset_level);

  stopAcq();

  m_cam->_setStatus(Camera::Ready,true);
}

void Interface::prepareAcq()
{
  DEB_MEMBER_FUNCT();
  m_cam->prepareAcq();
}

void Interface::startAcq()
{
	DEB_MEMBER_FUNCT();
	m_cam->startAcq();
}

void Interface::stopAcq()
{
  DEB_MEMBER_FUNCT();
  m_cam->stopAcq();
}

void Interface::getStatus(StatusType& status)
{
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
    }
    status.det_mask = DetExposure | DetReadout | DetLatency;
}

int Interface::getNbHwAcquiredFrames()
{
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

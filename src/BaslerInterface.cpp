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

#include "BaslerInterface.h"
#include "BaslerCamera.h"
#include "BaslerDetInfoCtrlObj.h"
#include "BaslerSyncCtrlObj.h"
#include "BaslerRoiCtrlObj.h"
#include "BaslerBinCtrlObj.h"
#include "BaslerVideoCtrlObj.h"

using namespace lima;
using namespace lima::Basler;


Interface::Interface(Camera& cam,bool force_video_mode) :
  m_cam(cam)
{
  DEB_CONSTRUCTOR();
  DEB_ALWAYS() << DEB_VAR1(force_video_mode);
  m_det_info = new DetInfoCtrlObj(cam);
  m_sync = new SyncCtrlObj(cam);
  m_roi = new RoiCtrlObj(cam);
  m_bin = new BinCtrlObj(cam);
  bool has_video_capability;
  m_cam.hasVideoCapability(has_video_capability);
  if(has_video_capability || force_video_mode)
    {
      if(!has_video_capability) {
	// for greyscale camera but for having true video interface
	// with gain/autogain and other true video features available
	// one can force here for video interface.
	DEB_ALWAYS() << "Ok force video cap. for a B/W camera";
	m_cam._forceVideoMode(true);
      }
      m_video = new VideoCtrlObj(cam);
    }
  else
    m_video = NULL;
}

Interface::~Interface()
{
  DEB_DESTRUCTOR();
  delete m_det_info;
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
}

void Interface::reset(ResetLevel reset_level)
{
  DEB_MEMBER_FUNCT();
  DEB_PARAM() << DEB_VAR1(reset_level);

  stopAcq();

  m_cam._setStatus(Camera::Ready,true);
}

void Interface::prepareAcq()
{
  DEB_MEMBER_FUNCT();
  m_cam.prepareAcq();
}

void Interface::startAcq()
{
  DEB_MEMBER_FUNCT();
  m_cam.startAcq();
}

void Interface::stopAcq()
{
  DEB_MEMBER_FUNCT();
  m_cam.stopAcq();
}

void Interface::getStatus(StatusType& status)
{
  status.det = DetIdle;
  Camera::Status basler_status;
  m_cam.getStatus(basler_status);
  switch (basler_status)
    {
    case Camera::Exposure:
      status.det = DetExposure;
      status.acq = AcqRunning;
      break;
    case Camera::Readout:
      status.det = DetReadout;
      status.acq = AcqRunning; 
      break;
    case Camera::Latency:
      status.det = DetLatency;
      status.acq = AcqRunning;
      break;
    case Camera::WaitForTrigger:
      status.det = DetWaitForTrigger;
      TrigMode trig_mode;
      m_cam.getTrigMode(trig_mode);
      if (trig_mode  == IntTrigMult)
	status.det = DetIdle;
      else
	status.det = DetExposure;
      status.acq = AcqRunning;
      break;
    case Camera::Ready:
      status.acq = AcqReady;
      status.det = DetIdle;
      break;
    case Camera::Fault:
      status.acq = AcqFault;
      status.det = DetFault;
      break;
    }
}

int Interface::getNbHwAcquiredFrames()
{
  DEB_MEMBER_FUNCT();
  int acq_frames;
  m_cam.getNbHwAcquiredFrames(acq_frames);
  return acq_frames;
}

//-----------------------------------------------------

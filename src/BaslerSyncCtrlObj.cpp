#include <sstream>
#include "BaslerSyncCtrlObj.h"
#include "BaslerCamera.h"

using namespace lima;
using namespace lima::Basler;

SyncCtrlObj::SyncCtrlObj(Camera *cam) :
  m_cam(cam)
{
}

SyncCtrlObj::~SyncCtrlObj()
{
}

bool SyncCtrlObj::checkTrigMode(TrigMode trig_mode)
{
    bool valid_mode = false;
    switch (trig_mode)
    {
        case IntTrig:
        case ExtTrigSingle:
        case ExtGate:
            valid_mode = true;
        break;

        default:
            valid_mode = false;
        break;
    }
    return valid_mode;  
}

void SyncCtrlObj::setTrigMode(TrigMode trig_mode)
{
    DEB_MEMBER_FUNCT();    
    if (!checkTrigMode(trig_mode))
        THROW_HW_ERROR(InvalidValue) << "Invalid " << DEB_VAR1(trig_mode);
    m_cam->setTrigMode(trig_mode);
}

void SyncCtrlObj::getTrigMode(TrigMode &trig_mode)
{
  m_cam->getTrigMode(trig_mode);
}

void SyncCtrlObj::setExpTime(double exp_time)
{
  m_cam->setExpTime(exp_time);
}

void SyncCtrlObj::getExpTime(double &exp_time)
{
	m_cam->getExpTime(exp_time);
}

void SyncCtrlObj::setLatTime(double  lat_time)
{
  m_cam->setLatTime(lat_time);
}

void SyncCtrlObj::getLatTime(double& lat_time)
{
	m_cam->getLatTime(lat_time);
}

void SyncCtrlObj::setNbHwFrames(int  nb_frames)
{
	m_cam->setNbFrames(nb_frames);
}

void SyncCtrlObj::getNbHwFrames(int& nb_frames)
{
	m_cam->getNbFrames(nb_frames);
}

void SyncCtrlObj::getValidRanges(ValidRangesType& valid_ranges)
{
    DEB_MEMBER_FUNCT();
    double min_time;
    double max_time;

    m_cam->getExposureTimeRange(min_time, max_time);
    valid_ranges.min_exp_time = min_time;
    valid_ranges.max_exp_time = max_time;

    m_cam->getLatTimeRange(min_time, max_time);
    valid_ranges.min_lat_time = min_time;
    valid_ranges.max_lat_time = max_time;
}
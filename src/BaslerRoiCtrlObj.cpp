#include <sstream>
#include "BaslerRoiCtrlObj.h"
#include "BaslerCamera.h"

using namespace lima;
using namespace lima::Basler;

RoiCtrlObj::RoiCtrlObj(Camera *cam) :
  m_cam(cam)
{
}

RoiCtrlObj::~RoiCtrlObj()
{
}
void RoiCtrlObj::checkRoi(const Roi& set_roi, Roi& hw_roi)
{
    DEB_MEMBER_FUNCT();
    m_cam->checkRoi(set_roi, hw_roi);
}

void RoiCtrlObj::setRoi(const Roi& roi)
{
    DEB_MEMBER_FUNCT();
    Roi real_roi;
    checkRoi(roi,real_roi);
    m_cam->setRoi(real_roi);
}

void RoiCtrlObj::getRoi(Roi& roi)
{
    DEB_MEMBER_FUNCT();
    m_cam->getRoi(roi);
}

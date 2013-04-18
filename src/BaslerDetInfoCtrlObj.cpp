#include <cstdlib>
#include "BaslerDetInfoCtrlObj.h"
#include "BaslerCamera.h"

using namespace lima;
using namespace lima::Basler;

DetInfoCtrlObj::DetInfoCtrlObj(Camera *cam):
  m_cam(cam)
{
}

DetInfoCtrlObj::~DetInfoCtrlObj()
{
}

void DetInfoCtrlObj::getMaxImageSize(Size& max_image_size)
{
    m_cam->getDetectorImageSize(max_image_size);
}

void DetInfoCtrlObj::getDetectorImageSize(Size& det_image_size)
{
    m_cam->getDetectorImageSize(det_image_size);
}

void DetInfoCtrlObj::getDefImageType(ImageType& def_image_type)
{
    m_cam->getImageType(def_image_type);
}

void DetInfoCtrlObj::getCurrImageType(ImageType& curr_image_type)
{
    m_cam->getImageType(curr_image_type);
}

void DetInfoCtrlObj::setCurrImageType(ImageType curr_image_type)
{
    m_cam->setImageType(curr_image_type);
}

void DetInfoCtrlObj::getPixelSize(double& x_size,double& y_size)
{
    x_size = y_size = 55.0e-6;
}

void DetInfoCtrlObj::getDetectorType(std::string& det_type)
{
    m_cam->getDetectorType(det_type);
}

void DetInfoCtrlObj::getDetectorModel(std::string& det_model)
{
    m_cam->getDetectorModel(det_model);
}

void DetInfoCtrlObj::registerMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
{
    //m_cam.registerMaxImageSizeCallback(cb);
}

void DetInfoCtrlObj::unregisterMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
{
    //m_cam.unregisterMaxImageSizeCallback(cb);
}
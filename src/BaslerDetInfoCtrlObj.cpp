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
#include <cstdlib>
#include "BaslerDetInfoCtrlObj.h"
#include "BaslerCamera.h"

using namespace lima;
using namespace lima::Basler;

DetInfoCtrlObj::DetInfoCtrlObj(Camera& cam):
  m_cam(cam)
{
}

DetInfoCtrlObj::~DetInfoCtrlObj()
{
}

void DetInfoCtrlObj::getMaxImageSize(Size& max_image_size)
{
    m_cam.getDetectorImageSize(max_image_size);
}

void DetInfoCtrlObj::getDetectorImageSize(Size& det_image_size)
{
    m_cam.getDetectorImageSize(det_image_size);
}

void DetInfoCtrlObj::getDefImageType(ImageType& def_image_type)
{
    m_cam.getImageType(def_image_type);
}

void DetInfoCtrlObj::getCurrImageType(ImageType& curr_image_type)
{
    m_cam.getImageType(curr_image_type);
}

void DetInfoCtrlObj::setCurrImageType(ImageType curr_image_type)
{
    m_cam.setImageType(curr_image_type);
}

void DetInfoCtrlObj::getPixelSize(double& x_size,double& y_size)
{
    x_size = y_size = 55.0e-6;
}

void DetInfoCtrlObj::getDetectorType(std::string& det_type)
{
    m_cam.getDetectorType(det_type);
}

void DetInfoCtrlObj::getDetectorModel(std::string& det_model)
{
    m_cam.getDetectorModel(det_model);
}

void DetInfoCtrlObj::registerMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
{
    //m_cam.registerMaxImageSizeCallback(cb);
}

void DetInfoCtrlObj::unregisterMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
{
    //m_cam.unregisterMaxImageSizeCallback(cb);
}

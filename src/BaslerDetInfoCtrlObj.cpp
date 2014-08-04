<<<<<<< HEAD
=======
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
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
#include <cstdlib>
#include "BaslerDetInfoCtrlObj.h"
#include "BaslerCamera.h"

using namespace lima;
using namespace lima::Basler;

<<<<<<< HEAD
DetInfoCtrlObj::DetInfoCtrlObj(Camera *cam):
=======
DetInfoCtrlObj::DetInfoCtrlObj(Camera& cam):
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
  m_cam(cam)
{
}

DetInfoCtrlObj::~DetInfoCtrlObj()
{
}

void DetInfoCtrlObj::getMaxImageSize(Size& max_image_size)
{
<<<<<<< HEAD
    m_cam->getDetectorImageSize(max_image_size);
=======
    m_cam.getDetectorImageSize(max_image_size);
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void DetInfoCtrlObj::getDetectorImageSize(Size& det_image_size)
{
<<<<<<< HEAD
    m_cam->getDetectorImageSize(det_image_size);
=======
    m_cam.getDetectorImageSize(det_image_size);
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void DetInfoCtrlObj::getDefImageType(ImageType& def_image_type)
{
<<<<<<< HEAD
    m_cam->getImageType(def_image_type);
=======
    m_cam.getImageType(def_image_type);
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void DetInfoCtrlObj::getCurrImageType(ImageType& curr_image_type)
{
<<<<<<< HEAD
    m_cam->getImageType(curr_image_type);
=======
    m_cam.getImageType(curr_image_type);
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void DetInfoCtrlObj::setCurrImageType(ImageType curr_image_type)
{
<<<<<<< HEAD
    m_cam->setImageType(curr_image_type);
=======
    m_cam.setImageType(curr_image_type);
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void DetInfoCtrlObj::getPixelSize(double& x_size,double& y_size)
{
    x_size = y_size = 55.0e-6;
}

void DetInfoCtrlObj::getDetectorType(std::string& det_type)
{
<<<<<<< HEAD
    m_cam->getDetectorType(det_type);
=======
    m_cam.getDetectorType(det_type);
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void DetInfoCtrlObj::getDetectorModel(std::string& det_model)
{
<<<<<<< HEAD
    m_cam->getDetectorModel(det_model);
=======
    m_cam.getDetectorModel(det_model);
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void DetInfoCtrlObj::registerMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
{
    //m_cam.registerMaxImageSizeCallback(cb);
}

void DetInfoCtrlObj::unregisterMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
{
    //m_cam.unregisterMaxImageSizeCallback(cb);
<<<<<<< HEAD
}
=======
}
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25

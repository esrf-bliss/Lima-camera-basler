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
#include <sstream>
#include "BaslerBinCtrlObj.h"
#include "BaslerCamera.h"

using namespace lima;
using namespace lima::Basler;

<<<<<<< HEAD
BinCtrlObj::BinCtrlObj(Camera *cam) :
=======
BinCtrlObj::BinCtrlObj(Camera& cam) :
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
  m_cam(cam)
{
}

BinCtrlObj::~BinCtrlObj()
{
}
void BinCtrlObj::setBin(const Bin& aBin)
{
<<<<<<< HEAD
  m_cam->setBin(aBin);
=======
  m_cam.setBin(aBin);
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void BinCtrlObj::getBin(Bin &aBin)
{
<<<<<<< HEAD
  m_cam->getBin(aBin);
=======
  m_cam.getBin(aBin);
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
}

void BinCtrlObj::checkBin(Bin &aBin)
{
<<<<<<< HEAD
  m_cam->checkBin(aBin);
}
=======
  m_cam.checkBin(aBin);
}
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25

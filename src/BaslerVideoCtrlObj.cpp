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
#include "BaslerVideoCtrlObj.h"
#include "BaslerCamera.h"
#include "BaslerInterface.h"

using namespace lima;
using namespace lima::Basler;

VideoCtrlObj::VideoCtrlObj(Camera& cam) :
  m_cam(cam)
{
  m_cam.m_video = this;
}

VideoCtrlObj::~VideoCtrlObj()
{
}

void VideoCtrlObj::getSupportedVideoMode(std::list<VideoMode>& aList) const
{
  DEB_MEMBER_FUNCT();
  struct _VideoMode
  {
    const char* stringMode;
    VideoMode   mode;
  };
  static const _VideoMode BaslerVideoMode[] =
    {
      {"BayerRG16",BAYER_RG16},
      {"BayerBG16",BAYER_BG16},
      {"BayerRG12",BAYER_RG16},
      {"BayerBG12",BAYER_BG16},
      {"BayerRG8",BAYER_RG8},
      {"BayerBG8",BAYER_BG8},
      {"RGB8Packed",RGB24},
      {"BGR8Packed",BGR24},
      {"YUV411Packed",YUV411PACKED},
      {"YUV422Packed",YUV422PACKED},
      {"YUV444Packed",YUV444PACKED},
      {"Mono16",Y16},
      {"Mono12",Y16},
      {"Mono8",Y8},
      {NULL,Y8}
    };
  for(const _VideoMode* pt = BaslerVideoMode;pt->stringMode;++pt)
    {
      GenApi::IEnumEntry *anEntry = 
	m_cam.Camera_->PixelFormat.GetEntryByName(pt->stringMode);
      if(anEntry && GenApi::IsAvailable(anEntry))
	aList.push_back(pt->mode);
    }
}
void VideoCtrlObj::getVideoMode(VideoMode &mode) const
{
  DEB_MEMBER_FUNCT();

  PixelFormatEnums aCurrentPixelFormat = m_cam.Camera_->PixelFormat.GetValue();
  switch(aCurrentPixelFormat)
    {
    case PixelFormat_Mono8:             mode = Y8;		break;
    case PixelFormat_Mono10: 		mode = Y16;		break;
    case PixelFormat_Mono12:  		mode = Y16;		break;
    case PixelFormat_Mono16:  		mode = Y16;		break;
    case PixelFormat_BayerRG8:  	mode = BAYER_RG8;	break;
    case PixelFormat_BayerBG8: 		mode = BAYER_BG8;	break;  
    case PixelFormat_BayerRG10:  	mode = BAYER_RG16;	break;
    case PixelFormat_BayerBG10:    	mode = BAYER_BG16;	break;
    case PixelFormat_BayerRG12:    	mode = BAYER_RG16;	break;
    case PixelFormat_BayerBG12:      	mode = BAYER_BG16;	break;
    case PixelFormat_RGB8Packed:  	mode = RGB24;		break;
    case PixelFormat_BGR8Packed:  	mode = BGR24;		break;
    case PixelFormat_RGBA8Packed:  	mode = RGB32;		break;
    case PixelFormat_BGRA8Packed:  	mode = BGR32;		break;
    case PixelFormat_YUV411Packed:  	mode = YUV411PACKED;	break;
    case PixelFormat_YUV422Packed:  	mode = YUV422PACKED;	break;
    case PixelFormat_YUV444Packed:  	mode = YUV444PACKED;	break;
    case PixelFormat_BayerRG16:    	mode = BAYER_RG16;	break;
    case PixelFormat_BayerBG16:    	mode = BAYER_BG16;	break;
    default:
      THROW_HW_ERROR(NotSupported) << "Pixel type not supported yet";
    }
}

void VideoCtrlObj::setVideoMode(VideoMode mode)
{
  DEB_MEMBER_FUNCT();

  std::list<PixelFormatEnums> pixelformat;
  switch(mode)
    {
    case Y8:
      pixelformat.push_back(PixelFormat_Mono8);
      break;
    case Y16: 
      pixelformat.push_back(PixelFormat_Mono16);
      pixelformat.push_back(PixelFormat_Mono12);
      pixelformat.push_back(PixelFormat_Mono10);
      break;
    case BAYER_RG8: 
      pixelformat.push_back(PixelFormat_BayerRG8);
      break;
    case BAYER_BG8: 
      pixelformat.push_back(PixelFormat_BayerBG8);
      break;  
    case BAYER_RG16: 
      pixelformat.push_back(PixelFormat_BayerRG16);
      pixelformat.push_back(PixelFormat_BayerRG12);
      pixelformat.push_back(PixelFormat_BayerRG10);
      break;
    case BAYER_BG16:
      pixelformat.push_back(PixelFormat_BayerBG16);
      pixelformat.push_back(PixelFormat_BayerBG12);
      pixelformat.push_back(PixelFormat_BayerBG10);
      break;
    case RGB24: 
      pixelformat.push_back(PixelFormat_RGB8Packed);
      break;
    case BGR24:
      pixelformat.push_back(PixelFormat_BGR8Packed);
      break;
    case RGB32:
      pixelformat.push_back(PixelFormat_RGBA8Packed);
      break;
    case BGR32:
      pixelformat.push_back(PixelFormat_BGRA8Packed);
      break;
    case YUV411PACKED:
      pixelformat.push_back(PixelFormat_YUV411Packed);
      break;
    case YUV422PACKED:
      pixelformat.push_back(PixelFormat_YUV422Packed);
      break;
    case YUV444PACKED:
      pixelformat.push_back(PixelFormat_YUV444Packed);
      break;
    default:
      THROW_HW_ERROR(NotSupported) << "Mode type not supported yet";
    }
 
  bool succeed = false;
  std::string errorMsg;
  for(std::list<PixelFormatEnums>::iterator i = pixelformat.begin();
      !succeed && i != pixelformat.end();++i)
    {
      try
	{
	  m_cam.Camera_->PixelFormat.SetValue(*i);
	  succeed = true;
	}
      catch (Pylon::GenericException &e)
	{
	  errorMsg += e.GetDescription() + '\n';
	}
    }
 
  if(!succeed)
    THROW_HW_ERROR(Error) << errorMsg;
}

void VideoCtrlObj::setLive(bool flag)
{
  if(flag)
    {
      m_cam.setNbFrames(0);
      m_cam.prepareAcq();
      m_cam.startAcq();
    }
  else
    m_cam.stopAcq();
}

void VideoCtrlObj::getLive(bool &flag) const
{
  
}

void VideoCtrlObj::getGain(double &aGain) const
{
  m_cam.getGain(aGain);
}

void VideoCtrlObj::setGain(double aGain)
{
  m_cam.setGain(aGain);
}

void VideoCtrlObj::checkBin(Bin &bin)
{
  bin = Bin(1,1);		// Do not manage Hw Bin
}

void VideoCtrlObj::checkRoi(const Roi&,Roi& hw_roi)
{
  hw_roi = Roi(0,0,
	       m_cam.Camera_->Width.GetMax(),
	       m_cam.Camera_->Height.GetMax());
}

bool VideoCtrlObj::checkAutoGainMode(AutoGainMode mode) const
{
  return mode == OFF ? true :
    GenApi::IsAvailable(m_cam.Camera_->GainAuto) && 
    GenApi::IsAvailable(m_cam.Camera_->GainSelector);
}

void VideoCtrlObj::setHwAutoGainMode(AutoGainMode mode)
{
  m_cam.setAutoGain(mode == ON);
}

#include <sstream>
#include "BaslerBinCtrlObj.h"
#include "BaslerCamera.h"

using namespace lima;
using namespace lima::Basler;

BinCtrlObj::BinCtrlObj(Camera *cam) :
  m_cam(cam)
{
}

BinCtrlObj::~BinCtrlObj()
{
}
void BinCtrlObj::setBin(const Bin& aBin)
{
  m_cam->setBin(aBin);
}

void BinCtrlObj::getBin(Bin &aBin)
{
  m_cam->getBin(aBin);
}

void BinCtrlObj::checkBin(Bin &aBin)
{
  m_cam->checkBin(aBin);
}
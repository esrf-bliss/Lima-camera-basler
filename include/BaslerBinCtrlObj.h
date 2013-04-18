#ifndef BASLERBINCTRLOBJ_H
#define BASLERBINCTRLOBJ_H

#include "BaslerCompatibility.h"
#include "HwBinCtrlObj.h"
#include "HwInterface.h"

namespace lima
{
	namespace Basler
	{
		class Camera;

		class BinCtrlObj : public HwBinCtrlObj
		{
			DEB_CLASS_NAMESPC(DebModCamera,"BinCtrlObj","Balser");
		public:
			BinCtrlObj(Camera*);
			virtual ~BinCtrlObj();

			virtual void setBin(const Bin& bin);
			virtual void getBin(Bin& bin);
			//allow all binning
			virtual void checkBin(Bin& bin);
		
		private:
			Camera*			m_cam;
		};
	} // namespace Basler
} // namespace lima

#endif // BASLERBINCTRLOBJ_H

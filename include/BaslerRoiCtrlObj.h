#ifndef BASLERROICTRLOBJ_H
#define BASLERROICTRLOBJ_H

#include "BaslerCompatibility.h"
#include "HwRoiCtrlObj.h"
#include "HwInterface.h"

namespace lima
{
	namespace Basler
	{
		class Camera;

		class RoiCtrlObj : public HwRoiCtrlObj
		{
			DEB_CLASS_NAMESPC(DebModCamera,"RoiCtrlObj","Balser");
		public:
			RoiCtrlObj(Camera*);
			virtual ~RoiCtrlObj();

			virtual void setRoi(const Roi& set_roi);
			virtual void getRoi(Roi& hw_roi);
			virtual void checkRoi(const Roi& set_roi, Roi& hw_roi);
		private:
			Camera*			m_cam;
		};
	} // namespace Basler
} // namespace lima

#endif // BASLERROICTRLOBJ_H

#ifndef BASLERSYNCCTRLOBJ_H
#define BASLERSYNCCTRLOBJ_H

#include "BaslerCompatibility.h"
#include "HwSyncCtrlObj.h"
#include "HwInterface.h"

namespace lima
{
  namespace Basler
  {
    class Camera;

    class SyncCtrlObj : public HwSyncCtrlObj
    {
      DEB_CLASS_NAMESPC(DebModCamera,"SyncCtrlObj","Balser");
    public:
      SyncCtrlObj(Camera*);
      virtual ~SyncCtrlObj();

      virtual bool checkTrigMode(TrigMode trig_mode);
      virtual void setTrigMode(TrigMode  trig_mode);
      virtual void getTrigMode(TrigMode& trig_mode);

      virtual void setExpTime(double  exp_time);
      virtual void getExpTime(double& exp_time);

      virtual void setLatTime(double  lat_time);
      virtual void getLatTime(double& lat_time);

      virtual void setNbHwFrames(int  nb_frames);
      virtual void getNbHwFrames(int& nb_frames);

      virtual void getValidRanges(ValidRangesType& valid_ranges);

      void startAcq();
      void stopAcq(bool clearQueue = true);
      
      void getStatus(HwInterface::StatusType&);

    private:
      Camera*			m_cam;
     // tPvHandle&		m_handle;
      TrigMode			m_trig_mode;
   //   BufferCtrlObj*	m_buffer;
      int				m_nb_frames;
      bool				m_started;
    };

  } // namespace Basler
} // namespace lima

#endif // BASLERSYNCCTRLOBJ_H

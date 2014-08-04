#ifndef BASLERINTERFACE_H
#define BASLERINTERFACE_H

#include "BaslerCompatibility.h"
#include "HwInterface.h"

namespace lima
{
<<<<<<< HEAD
	namespace Basler
	{
		//class Interface;
		class DetInfoCtrlObj;
		class SyncCtrlObj;
		class RoiCtrlObj;
		class BinCtrlObj;
		class Camera;

		class Interface : public HwInterface
		{
			DEB_CLASS_NAMESPC(DebModCamera, "BaslerInterface", "Basler");

		 public:
			Interface(Camera*);
			virtual ~Interface();

			//- From HwInterface
			virtual void    getCapList(CapList&) const;
			virtual void    reset(ResetLevel reset_level);
			virtual void    prepareAcq();
			virtual void    startAcq();
			virtual void    stopAcq();
			virtual void    getStatus(StatusType& status);
			virtual int     getNbHwAcquiredFrames();

			void            getFrameRate(double& frame_rate);
			void            setTimeout(int TO);
			void			setGain(double gain);
			void			getGain(double& gain) const;
			void			setAutoGain(bool auto_gain);
			void			getAutoGain(bool& auto_gain) const;
		 private:
			Camera*			m_cam;
			CapList			m_cap_list;
			DetInfoCtrlObj* m_det_info;
			SyncCtrlObj*	m_sync;
			RoiCtrlObj*		m_roi;
			BinCtrlObj*		m_bin;

			mutable Cond     m_cond;
		};
} // namespace Basler
=======
  namespace Basler
  {
    //class Interface;
    class DetInfoCtrlObj;
    class SyncCtrlObj;
    class RoiCtrlObj;
    class BinCtrlObj;
    class VideoCtrlObj;
    class Camera;
    class LIBBASLER_API Interface : public HwInterface
    {
      DEB_CLASS_NAMESPC(DebModCamera, "BaslerInterface", "Basler");

    public:
      Interface(Camera&);
      virtual ~Interface();

      //- From HwInterface
      virtual void	getCapList(CapList&) const;
      virtual void	reset(ResetLevel reset_level);
      virtual void	prepareAcq();
      virtual void	startAcq();
      virtual void	stopAcq();
      virtual void	getStatus(StatusType& status);
      virtual int	getNbHwAcquiredFrames();

      Camera& getCamera(){ return m_cam; }
      const Camera& getCamera() const { return m_cam; }
    private:
      Camera&		m_cam;
      CapList		m_cap_list;
      DetInfoCtrlObj*	m_det_info;
      SyncCtrlObj*	m_sync;
      RoiCtrlObj*	m_roi;
      BinCtrlObj*	m_bin;
      VideoCtrlObj*     m_video;
      mutable Cond	m_cond;
    };
  } // namespace Basler
>>>>>>> 8c39a25a0ded40896c66c1c181a557e0d9924a25
} // namespace lima

#endif // BASLERINTERFACE_H

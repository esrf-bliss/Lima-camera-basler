namespace Basler
{
  class Interface : HwInterface
  {
%TypeHeaderCode
#include <BaslerInterface.h>
%End
  public:
    Interface(Basler::Camera& cam);
    virtual ~Interface();

    //- From HwInterface
    //    virtual void 	getCapList(CapList& /Out/) const;
    virtual void	getCapList(std::vector<HwCap> &cap_list /Out/) const;
    virtual void	reset(ResetLevel reset_level);
    virtual void 	prepareAcq();
    virtual void 	startAcq();
    virtual void 	stopAcq();
    virtual void 	getStatus(StatusType& status /Out/);
    virtual int 	getNbHwAcquiredFrames();

    void		getFrameRate(double& frame_rate /Out/);
%MethodCode
	//backward compatibility
	sipCpp->getCamera().getFrameRate(a0);
%End
    void		setTimeout(int);
%MethodCode
	sipCpp->getCamera().setTimeout(a0);
%End
    void		setGain(double gain);
%MethodCode
	sipCpp->getCamera().setGain(a0);
%End
    void		getGain(double& gain /Out/) const;
%MethodCode
	sipCpp->getCamera().getGain(a0);
%End
    void		setAutoGain(bool auto_gain);
%MethodCode
	sipCpp->getCamera().setAutoGain(a0);
%End
    void		getAutoGain(bool& auto_gain /Out/) const;
%MethodCode
	sipCpp->getCamera().getAutoGain(a0);
%End
  };
};

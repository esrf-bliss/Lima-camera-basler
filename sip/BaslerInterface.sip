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
    void		setTimeout(int);
    void		setGain(double gain);
    void		getGain(double& gain /Out/) const;

    void		setAutoGain(bool auto_gain);
    void		getAutoGain(bool& auto_gain /Out/) const;
  };
};

#ifndef BASLERINTERFACE_H
#define BASLERINTERFACE_H

#include "HwInterface.h"
#include "BaslerCamera.h"

namespace lima
{
/*	
namespace Basler
{
*/
class Interface;

/*******************************************************************
 * \class DetInfoCtrlObj
 * \brief Control object providing Basler detector info interface
 *******************************************************************/

class DetInfoCtrlObj : public HwDetInfoCtrlObj
{
	DEB_CLASS_NAMESPC(DebModCamera, "DetInfoCtrlObj", "Basler");

 public:
	DetInfoCtrlObj(Camera& cam);
	virtual ~DetInfoCtrlObj();

	virtual void getMaxImageSize(Size& max_image_size);
	virtual void getDetectorImageSize(Size& det_image_size);

	virtual void getDefImageType(ImageType& def_image_type);
	virtual void getCurrImageType(ImageType& curr_image_type);
	virtual void setCurrImageType(ImageType  curr_image_type);

	virtual void getPixelSize(double& pixel_size);
	virtual void getDetectorType(std::string& det_type);
	virtual void getDetectorModel(std::string& det_model);

	virtual void registerMaxImageSizeCallback(HwMaxImageSizeCallback& cb);
	virtual void unregisterMaxImageSizeCallback(HwMaxImageSizeCallback& cb);

 private:
	Camera& m_cam;
};


/*******************************************************************
 * \class BufferCtrlObj
 * \brief Control object providing Basler buffering interface
 *******************************************************************/

class BufferCtrlObj : public HwBufferCtrlObj
{
	DEB_CLASS_NAMESPC(DebModCamera, "BufferCtrlObj", "Basler");

 public:
	BufferCtrlObj(Camera& simu);
	virtual ~BufferCtrlObj();

	virtual void setFrameDim(const FrameDim& frame_dim);
	virtual void getFrameDim(      FrameDim& frame_dim);

	virtual void setNbBuffers(int  nb_buffers);
	virtual void getNbBuffers(int& nb_buffers);

	virtual void setNbConcatFrames(int  nb_concat_frames);
	virtual void getNbConcatFrames(int& nb_concat_frames);

	virtual void setNbAccFrames(int  nb_acc_frames);
	virtual void getNbAccFrames(int& nb_acc_frames);

	virtual void getMaxNbBuffers(int& max_nb_buffers);

	virtual void *getBufferPtr(int buffer_nb, int concat_frame_nb = 0);
	virtual void *getFramePtr(int acq_frame_nb);

	virtual void getStartTimestamp(Timestamp& start_ts);
	virtual void getFrameInfo(int acq_frame_nb, HwFrameInfoType& info);

	virtual void registerFrameCallback(HwFrameCallback& frame_cb);
	virtual void unregisterFrameCallback(HwFrameCallback& frame_cb);

 private:
	BufferCtrlMgr& m_buffer_mgr;
};

/*******************************************************************
 * \class SyncCtrlObj
 * \brief Control object providing Maxipix synchronization interface
 *******************************************************************/

class SyncCtrlObj : public HwSyncCtrlObj
{
    DEB_CLASS_NAMESPC(DebModCamera, "SyncCtrlObj", "Basler");

  public:
	SyncCtrlObj(Camera& cam, HwBufferCtrlObj& buffer_ctrl);
    virtual ~SyncCtrlObj();

    virtual void setTrigMode(TrigMode  trig_mode);
    virtual void getTrigMode(TrigMode& trig_mode);

    virtual void setExpTime(double  exp_time);
    virtual void getExpTime(double& exp_time);

    virtual void setLatTime(double  lat_time);
    virtual void getLatTime(double& lat_time);

    virtual void setNbHwFrames(int  nb_frames);
    virtual void getNbHwFrames(int& nb_frames);

    virtual void getValidRanges(ValidRangesType& valid_ranges);

  private:
    Camera& m_cam;
};



/*******************************************************************
 * \class FlipCtrlObj
 * \brief Control object providing Frelon flip interface
 *******************************************************************/

class FlipCtrlObj : public HwFlipCtrlObj
{
	DEB_CLASS_NAMESPC(DebModCamera, "FlipCtrlObj", "Frelon");

 public:
	FlipCtrlObj(Camera& cam);
	virtual ~FlipCtrlObj();

	virtual void setFlip(const Flip& flip);
	virtual void getFlip(Flip& flip);
	virtual void checkFlip(Flip& flip);

 private:
	Camera& m_cam;
};



/*******************************************************************
 * \class Interface
 * \brief Basler hardware interface
 *******************************************************************/

class BaslerInterface : public HwInterface
{
	DEB_CLASS_NAMESPC(DebModCamera, "BaslerInterface", "Basler");

 public:
	BaslerInterface(Camera& cam);
	virtual ~BaslerInterface();

	//- From HwInterface
	virtual void 	getCapList(CapList&) const;
	virtual void	reset(ResetLevel reset_level);
	virtual void 	prepareAcq();
	virtual void 	startAcq();
	virtual void 	stopAcq();
	virtual void 	getStatus(StatusType& status);
	virtual int 	getNbHwAcquiredFrames();

	void 			getFrameRate(double& frame_rate);
 private:
	Camera&			m_cam;
	CapList 		m_cap_list;
	DetInfoCtrlObj	m_det_info;
	BufferCtrlObj	m_buffer;
	SyncCtrlObj		m_sync;
	//BinCtrlObj     m_bin;
	//RoiCtrlObj     m_roi;
	FlipCtrlObj    m_flip;
};



/*
} // namespace Basler
*/
} // namespace lima

#endif // BASLERINTERFACE_H

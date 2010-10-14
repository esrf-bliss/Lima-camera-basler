#include "BaslerInterface.h"
#include <algorithm>

using namespace lima;
//using namespace lima::Basler;
using namespace std;

/*******************************************************************
 * \brief DetInfoCtrlObj constructor
 *******************************************************************/
DetInfoCtrlObj::DetInfoCtrlObj(Camera& cam)
	       :m_cam(cam)
{
    DEB_CONSTRUCTOR();
}

DetInfoCtrlObj::~DetInfoCtrlObj()
{
    DEB_DESTRUCTOR();
}

void DetInfoCtrlObj::getMaxImageSize(Size& size)
{
    DEB_MEMBER_FUNCT();
    m_cam.getImageSize(size);
}

void DetInfoCtrlObj::getDetectorImageSize(Size& size)
{
    DEB_MEMBER_FUNCT();
    m_cam.getImageSize(size);
}

void DetInfoCtrlObj::getDefImageType(ImageType& image_type)
{
    DEB_MEMBER_FUNCT();
	m_cam.getImageType(image_type);
}

void DetInfoCtrlObj::getCurrImageType(ImageType& image_type)
{
    DEB_MEMBER_FUNCT();
	m_cam.getImageType(image_type);
}

void DetInfoCtrlObj::setCurrImageType(ImageType image_type)
{
    DEB_MEMBER_FUNCT();
    ImageType valid_image_type;
    getDefImageType(valid_image_type);
    if (image_type != valid_image_type)
	THROW_HW_ERROR(Error) << "Cannot change to "  << DEB_VAR2(image_type, valid_image_type);
}

void DetInfoCtrlObj::getPixelSize(double& size)
{
    DEB_MEMBER_FUNCT();
    m_cam.getPixelSize(size);
}

void DetInfoCtrlObj::getDetectorType(std::string& type)
{
    DEB_MEMBER_FUNCT();
    m_cam.getDetectorType(type);
}

void DetInfoCtrlObj::getDetectorModel(std::string& model)
{
    DEB_MEMBER_FUNCT();
    m_cam.getDetectorModel(model);
}

void DetInfoCtrlObj::registerMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
{
    DEB_MEMBER_FUNCT();
    m_cam.registerMaxImageSizeCallback(cb);
}

void DetInfoCtrlObj::unregisterMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
{
    DEB_MEMBER_FUNCT();
    m_cam.unregisterMaxImageSizeCallback(cb);
}




/*******************************************************************
 * \brief BufferCtrlObj constructor
 *******************************************************************/

BufferCtrlObj::BufferCtrlObj(Camera& cam)
	: m_buffer_mgr(cam.getBufferMgr())
{
	DEB_CONSTRUCTOR();
}


BufferCtrlObj::~BufferCtrlObj()
{
	DEB_DESTRUCTOR();
}

void BufferCtrlObj::setFrameDim(const FrameDim& frame_dim)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.setFrameDim(frame_dim);
}

void BufferCtrlObj::getFrameDim(FrameDim& frame_dim)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.getFrameDim(frame_dim);
}

void BufferCtrlObj::setNbBuffers(int nb_buffers)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.setNbBuffers(nb_buffers);
}

void BufferCtrlObj::getNbBuffers(int& nb_buffers)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.getNbBuffers(nb_buffers);
}

void BufferCtrlObj::setNbConcatFrames(int nb_concat_frames)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.setNbConcatFrames(nb_concat_frames);
}

void BufferCtrlObj::getNbConcatFrames(int& nb_concat_frames)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.getNbConcatFrames(nb_concat_frames);
}

void BufferCtrlObj::setNbAccFrames(int nb_acc_frames)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.setNbAccFrames(nb_acc_frames);
}

void BufferCtrlObj::getNbAccFrames(int& nb_acc_frames)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.getNbAccFrames(nb_acc_frames);
}

void BufferCtrlObj::getMaxNbBuffers(int& max_nb_buffers)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.getMaxNbBuffers(max_nb_buffers);
}

void *BufferCtrlObj::getBufferPtr(int buffer_nb, int concat_frame_nb)
{
	DEB_MEMBER_FUNCT();
	return m_buffer_mgr.getBufferPtr(buffer_nb, concat_frame_nb);
}

void *BufferCtrlObj::getFramePtr(int acq_frame_nb)
{
	DEB_MEMBER_FUNCT();
	return m_buffer_mgr.getFramePtr(acq_frame_nb);
}

void BufferCtrlObj::getStartTimestamp(Timestamp& start_ts)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.getStartTimestamp(start_ts);
}

void BufferCtrlObj::getFrameInfo(int acq_frame_nb, HwFrameInfoType& info)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.getFrameInfo(acq_frame_nb, info);
}

void BufferCtrlObj::registerFrameCallback(HwFrameCallback& frame_cb)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.registerFrameCallback(frame_cb);
}

void BufferCtrlObj::unregisterFrameCallback(HwFrameCallback& frame_cb)
{
	DEB_MEMBER_FUNCT();
	m_buffer_mgr.unregisterFrameCallback(frame_cb);
}



/*******************************************************************
 * \brief SyncCtrlObj constructor
 *******************************************************************/

SyncCtrlObj::SyncCtrlObj(Camera& cam, HwBufferCtrlObj& buffer_ctrl)
	: HwSyncCtrlObj(buffer_ctrl), m_cam(cam)
{
}

SyncCtrlObj::~SyncCtrlObj()
{
}

void SyncCtrlObj::setTrigMode(TrigMode trig_mode)
{
	trig_mode = IntTrig;
}

void SyncCtrlObj::getTrigMode(TrigMode& trig_mode)
{
	if (trig_mode != IntTrig)
		throw LIMA_HW_EXC(InvalidValue, "Invalid (external) trigger");
}

void SyncCtrlObj::setExpTime(double exp_time)
{
	m_cam.setExpTime(exp_time);
}

void SyncCtrlObj::getExpTime(double& exp_time)
{
	m_cam.getExpTime(exp_time);
}

void SyncCtrlObj::setLatTime(double lat_time)
{
	m_cam.setLatTime(lat_time);
}

void SyncCtrlObj::getLatTime(double& lat_time)
{
	m_cam.getLatTime(lat_time);
}

void SyncCtrlObj::setNbHwFrames(int nb_frames)
{
	m_cam.setNbFrames(nb_frames);
}

void SyncCtrlObj::getNbHwFrames(int& nb_frames)
{
	m_cam.getNbFrames(nb_frames);
}

void SyncCtrlObj::getValidRanges(ValidRangesType& valid_ranges)
{
	double min_time = 10e-9;;
	double max_time = 1e6;
	valid_ranges.min_exp_time = min_time;
	valid_ranges.max_exp_time = max_time;
	valid_ranges.min_lat_time = min_time;
	valid_ranges.max_lat_time = max_time;
}



/*******************************************************************
 * \brief FlipCtrlObj constructor
 *******************************************************************/

FlipCtrlObj::FlipCtrlObj(Camera& cam)
	: m_cam(cam)
{
	DEB_CONSTRUCTOR();
}

FlipCtrlObj::~FlipCtrlObj()
{
	DEB_DESTRUCTOR();
}

void FlipCtrlObj::setFlip(const Flip& flip)
{
	DEB_MEMBER_FUNCT();
	m_cam.setFlip(flip);
}

void FlipCtrlObj::getFlip(Flip& flip)
{
	DEB_MEMBER_FUNCT();
	m_cam.getFlip(flip);
}

void FlipCtrlObj::checkFlip(Flip& flip)
{
	DEB_MEMBER_FUNCT();
	m_cam.checkFlip(flip);
}

/*******************************************************************
 * \brief Hw Interface constructor
 *******************************************************************/

BaslerInterface::BaslerInterface(Camera& cam)
	: m_cam(cam),m_det_info(cam), m_buffer(cam),m_sync(cam, m_buffer), m_flip(cam)
{
	DEB_CONSTRUCTOR();

	HwDetInfoCtrlObj *det_info = &m_det_info;
	m_cap_list.push_back(HwCap(det_info));

	HwBufferCtrlObj *buffer = &m_buffer;
	m_cap_list.push_back(HwCap(buffer));
	
	HwSyncCtrlObj *sync = &m_sync;
	m_cap_list.push_back(HwCap(sync));
	
	HwFlipCtrlObj *flip = &m_flip;
	m_cap_list.push_back(HwCap(flip));	
}

BaslerInterface::~BaslerInterface()
{
	DEB_DESTRUCTOR();
}

void BaslerInterface::getCapList(HwInterface::CapList &cap_list) const
{
	DEB_MEMBER_FUNCT();
	cap_list = m_cap_list;
}

void BaslerInterface::reset(ResetLevel reset_level)
{
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(reset_level);

	stopAcq();

	Size image_size;
	m_det_info.getMaxImageSize(image_size);
	ImageType image_type;
	m_det_info.getDefImageType(image_type);
	FrameDim frame_dim(image_size, image_type);
	m_buffer.setFrameDim(frame_dim);

	m_buffer.setNbConcatFrames(1);
	m_buffer.setNbAccFrames(1);
	m_buffer.setNbBuffers(1);
}

void BaslerInterface::prepareAcq()
{
	DEB_MEMBER_FUNCT();
}

void BaslerInterface::startAcq()
{
	DEB_MEMBER_FUNCT();
	m_cam.start();
}

void BaslerInterface::stopAcq()
{
	DEB_MEMBER_FUNCT();
	m_cam.stop();
}

void BaslerInterface::getStatus(StatusType& status)
{
	cout<<"BaslerInterface::getStatus"<<endl;
	Camera::Status basler_status = Camera::Ready;
	m_cam.getStatus(basler_status);
	cout<<"basler_status = "<<basler_status<<endl;
	switch (basler_status)
	{
		case Camera::Ready:
			status.acq = AcqReady;
			status.det = DetIdle;
			break;
		case Camera::Exposure:
			status.det = DetExposure;
			goto Running;
		case Camera::Readout:
			status.det = DetReadout;
			goto Running;
		case Camera::Latency:
			status.det = DetLatency;
		Running:
			status.acq = AcqRunning;
			break;
	}
	status.det_mask = DetExposure | DetReadout | DetLatency;
}

int BaslerInterface::getNbHwAcquiredFrames()
{
	DEB_MEMBER_FUNCT();
	/*Acq::Status acq_status;
	m_acq.getStatus(acq_status);
	int nb_hw_acq_frames = acq_status.last_frame_nb + 1;
	DEB_RETURN() << DEB_VAR1(nb_hw_acq_frames);
	return nb_hw_acq_frames;*/
}


void BaslerInterface::getFrameRate(double& frame_rate)
{
	DEB_MEMBER_FUNCT();
	m_cam.getFrameRate(frame_rate);
}


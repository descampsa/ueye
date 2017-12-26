#ifndef UEYE_HPP
#define UEYE_HPP

extern "C"{
#include <ueye.h>
}
#include <exception>
#include <string>

#include <opencv2/core/core.hpp>

namespace ueye{

class Exception:public std::exception
{
	public:
	Exception(HIDS camera_handle, INT error_code, std::string context="");
	virtual ~Exception() throw();
	virtual const char* what() const throw();
	
	private:
	HIDS CameraHandle;
	INT ErrorCode;
	std::string ErrorDescription;
};

template<class T>
class Range
{
	public:
	Range(const T &min=0, const T &max=0, const T &step=0):
		Min(min), Max(max), Step(step)
	{}
	T min() const {return Min;}
	T max() const {return Max;}
	T step() const {return Step;}
	
	private:
	T Min, Max, Step;
};

class Camera;

class ImageMemory
{
	public:
	
	ImageMemory(const Camera& camera, uint32_t width=0, uint32_t height=0, int32_t color_mode=0);
	explicit ImageMemory(const ImageMemory &memory);
	~ImageMemory();
	ImageMemory& operator=(const ImageMemory &memory);
	
	char* ptr();
	int id() const;
	
	void copyToMat(cv::Mat &mat)const;
	
	private:
	
	void create(HIDS camera_handle, uint32_t width, uint32_t height, int32_t color_mode);
	void release();
	
	HIDS CameraHandle;
	char *MemoryPtr;
	int32_t MemoryId;
	uint32_t Width;
	uint32_t Height;
	uint8_t BitDepth;
	int32_t ColorMode;
};

class Camera
{
	public:
	explicit Camera(uint8_t camera_id=0);
	~Camera();
	
	std::string getSensorName() const;
	int getSensorWidth() const;
	int getSensorHeight() const;
	uint8_t getSensorColorMode() const;
	
	int32_t getAOIPosX() const;
	int32_t getAOIPosY() const;
	int32_t getAOIWidth() const;
	int32_t getAOIHeight() const;
	int32_t getColorMode() const;
	
	Range<uint32_t> getPixelClockRange() const;
	Range<double> getFrameTimeRange() const;
	Range<double> getExposureRange() const;
	uint32_t getPixelClock() const;
	double getFrameRate() const;
	double getExposure() const;
	void setPixelClock(uint32_t pixel_clock);
	void setFrameRate(double frame_rate);
	void setExposure(double exposure);
	
	void imageCapture(ImageMemory &image_memory);
	
	void videoCaptureStart(std::vector<ImageMemory> &buffer);
	void videoCaptureStop();
	ImageMemory* waitNextFrame(uint32_t timeout=1000);
	void unlockFrame(ImageMemory *frame);
	
	HIDS handle()const;
	
	private:
	Camera(const Camera&); // non construction-copyable
	Camera& operator=(const Camera&); // non copyable
	
	enum TimingUpdate{TIMING_INIT, TIMING_PIXEL_CLOCK, TIMING_FRAME_RATE, TIMING_EXPOSURE};
	void updateTimingInfo(TimingUpdate update);
	
	HIDS CameraHandle;
	SENSORINFO SensorInfo;
	IS_RECT AOI;
	int32_t ColorMode;
	Range<uint32_t> PixelClockRange;
	Range<double> FrameTimeRange;
	Range<double> ExposureRange;
	uint32_t PixelClock;
	double FrameRate;
	double Exposure;
	
	std::map<char*, ImageMemory*> SequencePtr;
};

}

#endif

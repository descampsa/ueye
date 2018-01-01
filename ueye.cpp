#include "ueye.hpp"
#include <iostream>

#define THROW_IF_ERROR(...) \
{ \
	INT err_ = (__VA_ARGS__); \
	if(err_ != IS_SUCCESS) \
		throw Exception(CameraHandle, err_, #__VA_ARGS__); \
}

namespace{
	uint8_t bitDepth(int32_t color_mode)
	{
		switch(color_mode & ~IS_CM_PREFER_PACKED_SOURCE_FORMAT)
		{
			case IS_CM_MONO16:
			case IS_CM_SENSOR_RAW16:
			case IS_CM_BGR565_PACKED:
			case IS_CM_UYVY_PACKED:
			case IS_CM_CBYCRY_PACKED:
				return 16;
			case IS_CM_MONO12:
			case IS_CM_SENSOR_RAW12:
				return 12;
			case IS_CM_MONO10:
			case IS_CM_SENSOR_RAW10:
				return 10;
			case IS_CM_MONO8:
			case IS_CM_SENSOR_RAW8:
				return 8;
			case IS_CM_RGB12_UNPACKED:
			case IS_CM_BGR12_UNPACKED:
				return 36;
			case IS_CM_RGB10_UNPACKED:
			case IS_CM_RGB10_PACKED:
			case IS_CM_BGR10_UNPACKED:
			case IS_CM_BGR10_PACKED:
				return 30;
			case IS_CM_RGB8_PACKED:
			case IS_CM_BGR8_PACKED:
			case IS_CM_RGB8_PLANAR:
				return 24;
			case IS_CM_RGBA12_UNPACKED:
			case IS_CM_BGRA12_UNPACKED:
				return 48;
			case IS_CM_RGBA8_PACKED:
			case IS_CM_RGBY8_PACKED:
			case IS_CM_BGRA8_PACKED:
			case IS_CM_BGRY8_PACKED:
				return 32;
			case IS_CM_BGR5_PACKED:
				return 15;
			default:
				return 0;
		}
	}
	
	void matType(int32_t color_mode, int &type, int &channel)
	{
		channel = 1;
		switch(color_mode & ~IS_CM_PREFER_PACKED_SOURCE_FORMAT)
		{
			case IS_CM_MONO16:
			case IS_CM_SENSOR_RAW16:
			case IS_CM_MONO12:
			case IS_CM_SENSOR_RAW12:
			case IS_CM_MONO10:
			case IS_CM_SENSOR_RAW10:
				type = CV_16U;
				return;
				
			case IS_CM_MONO8:
			case IS_CM_SENSOR_RAW8:
				type = CV_8U;
				return;
				
			case IS_CM_RGB10_PACKED:
			case IS_CM_BGR10_PACKED:
				type = CV_16UC3;
				return;
				
			case IS_CM_UYVY_PACKED:
			case IS_CM_CBYCRY_PACKED:
			case IS_CM_BGR565_PACKED:
			case IS_CM_BGR5_PACKED:
				type = CV_8UC2;
				return;
				
			case IS_CM_RGB8_PACKED:
			case IS_CM_BGR8_PACKED:
				type = CV_8UC3;
				return;
				
			case IS_CM_RGBA8_PACKED:
			case IS_CM_RGBY8_PACKED:
			case IS_CM_BGRA8_PACKED:
			case IS_CM_BGRY8_PACKED:
				type = CV_8UC4;
				return;
			
			case IS_CM_RGB12_UNPACKED:
			case IS_CM_BGR12_UNPACKED:
			case IS_CM_RGB10_UNPACKED:
			case IS_CM_BGR10_UNPACKED:
				type = CV_16U;
				channel = 3;
				return;
				
			case IS_CM_RGB8_PLANAR:
				type = CV_8U;
				channel = 3;
				return;
				
			case IS_CM_RGBA12_UNPACKED:
			case IS_CM_BGRA12_UNPACKED:
				type = CV_16U;
				channel = 4;
				return;
				
			default:
				type = CV_8U;
				return;
		}
	}
}

namespace ueye
{

Exception::Exception(HIDS camera_handle, INT error_code, std::string context):
	CameraHandle(camera_handle), ErrorCode(error_code)
{
	ErrorDescription = "Unknown error";
	if(ErrorCode == IS_CANT_OPEN_DEVICE)
	{
		ErrorDescription = "Can't open device";
	}
	else if(ErrorCode == IS_INVALID_CAMERA_HANDLE)
	{
		ErrorDescription = "Invalid camera handle";
	}
	else if(camera_handle)
	{
		INT last_error;
		IS_CHAR *last_error_str;
		is_GetError(camera_handle, &last_error, &last_error_str);
		if(last_error_str != NULL)
		{
			ErrorDescription = std::string(last_error_str);
		}
	}
	if(!context.empty())
	{
		ErrorDescription += ", in context : ";
		ErrorDescription += context;
	}
}

Exception::~Exception() throw()
{}

const char* Exception::what() const throw()
{
	return ErrorDescription.c_str();
}


ImageMemory::ImageMemory(const Camera& camera, uint32_t width, uint32_t height, int32_t color_mode):
	CameraHandle(0), MemoryPtr(NULL), MemoryId(0), Width(0), Height(0), BitDepth(0), ColorMode(0)
{
	HIDS camera_handle = camera.handle();
	if(width == 0)
		width = camera.getAOIWidth();
	if(height == 0)
		height = camera.getAOIHeight();
	if(color_mode == 0)
		color_mode = camera.getColorMode();
	create(camera_handle, width, height, color_mode);
}

ImageMemory::ImageMemory(const ImageMemory &memory):
	CameraHandle(0), MemoryPtr(NULL), MemoryId(0), Width(0), Height(0), BitDepth(0), ColorMode(0)
{
	*this = memory;
}

ImageMemory::~ImageMemory()
{
	release();
}

ImageMemory& ImageMemory::operator=(const ImageMemory &memory)
{
	if(this == &memory)
		return *this;
	if(memory.CameraHandle!=CameraHandle || memory.Width!=Width || memory.Height!=Height || memory.ColorMode!=ColorMode)
	{
		release();
		create(memory.CameraHandle, memory.Width, memory.Height, memory.ColorMode);
	}
	THROW_IF_ERROR(is_CopyImageMem(CameraHandle, memory.MemoryPtr, memory.MemoryId, MemoryPtr));
}

void ImageMemory::create(HIDS camera_handle, uint32_t width, uint32_t height, int32_t color_mode)
{
	CameraHandle = camera_handle;
	Width = width;
	Height = height;
	ColorMode = color_mode;
	BitDepth = bitDepth(ColorMode);
	THROW_IF_ERROR(is_AllocImageMem(CameraHandle, Width, Height, BitDepth, &MemoryPtr, &MemoryId));
}

void ImageMemory::release()
{
	if(MemoryPtr)
		is_FreeImageMem(CameraHandle, MemoryPtr, MemoryId);
	MemoryPtr = NULL;
}

char* ImageMemory::ptr()
{
	return MemoryPtr;
}

int ImageMemory::id() const
{
	return MemoryId;
}

uint32_t ImageMemory::width() const
{
	return Width;
}

uint32_t ImageMemory::height() const
{
	return Height;
}

void ImageMemory::copyToMat(cv::Mat &mat)const
{
	int mat_type, mat_channel;
	matType(ColorMode, mat_type,  mat_channel);
	mat.create(Height*mat_channel, Width, mat_type);
	THROW_IF_ERROR(is_CopyImageMem(CameraHandle, MemoryPtr, MemoryId, mat.ptr<char>()));
}


std::vector<CameraInfo> getCameraList()
{
	HIDS CameraHandle = 0;
	
	INT camera_number;
	THROW_IF_ERROR(is_GetNumberOfCameras(&camera_number));

	UEYE_CAMERA_LIST *camera_list = (UEYE_CAMERA_LIST*) new uint8_t[sizeof(DWORD)+camera_number*sizeof(UEYE_CAMERA_INFO)];
	camera_list->dwCount = camera_number;
	THROW_IF_ERROR(is_GetCameraList(camera_list));
	
	std::vector<CameraInfo> result;
	for(int i=0; i<camera_number; ++i)
	{
		CameraInfo info;
		info.CameraId = camera_list->uci[i].dwCameraID;
		info.DeviceId = camera_list->uci[i].dwDeviceID;
		info.SensorId = camera_list->uci[i].dwSensorID;
		info.InUse = camera_list->uci[i].dwInUse;
		info.SerialNumber = std::string(camera_list->uci[i].SerNo, 16);
		info.SerialNumber.resize(info.SerialNumber.find('\0'));
		info.ModelName = std::string(camera_list->uci[i].Model, 16);
		info.ModelName.resize(info.ModelName.find('\0'));
		info.FullModelName = std::string(camera_list->uci[i].FullModelName, 32);
		info.FullModelName.resize(info.FullModelName.find('\0'));
		info.CameraStatus = camera_list->uci[i].dwStatus;
		result.push_back(info);
	}
	
	delete[] camera_list;
	return  result;
}


Camera::Camera(uint8_t camera_id):
	CameraHandle(0), ColorMode(0)
{
	CameraHandle = camera_id;
	THROW_IF_ERROR(is_InitCamera(&CameraHandle, 0));
	THROW_IF_ERROR(is_GetSensorInfo(CameraHandle, &SensorInfo));
	THROW_IF_ERROR(is_AOI(CameraHandle, IS_AOI_IMAGE_GET_AOI, &AOI, sizeof(AOI)));
	ColorMode = is_SetColorMode(CameraHandle, IS_GET_COLOR_MODE);
	updateTimingInfo(TIMING_INIT);
}

Camera::~Camera()
{
	is_ExitCamera(CameraHandle);
}

std::string Camera::getSensorName()const
{
	return std::string(SensorInfo.strSensorName, SensorInfo.strSensorName+32);
}

int Camera::getSensorWidth() const
{
	return SensorInfo.nMaxWidth;
}

int Camera::getSensorHeight() const
{
	return SensorInfo.nMaxHeight;
}

uint8_t Camera::getSensorColorMode() const
{
	return SensorInfo.nColorMode;
}

int32_t Camera::getAOIPosX() const
{
	return AOI.s32X;
}
int32_t Camera::getAOIPosY() const
{
	return AOI.s32Y;
}
int32_t Camera::getAOIWidth() const
{
	return AOI.s32Width;
}
int32_t Camera::getAOIHeight() const
{
	return AOI.s32Height;
}

int32_t Camera::getColorMode() const
{
	return ColorMode;
}

Range<uint32_t> Camera::getPixelClockRange() const
{
	return PixelClockRange;
}
Range<double> Camera::getFrameTimeRange() const
{
	return FrameTimeRange;
}
Range<double> Camera::getExposureRange() const
{
	return ExposureRange;
}
uint32_t Camera::getPixelClock() const
{
	return PixelClock;
}
double Camera::getFrameRate() const
{
	return FrameRate;
}
double Camera::getExposure() const
{
	return Exposure;
}
void Camera::setPixelClock(uint32_t pixel_clock)
{
	THROW_IF_ERROR(is_PixelClock(CameraHandle, IS_PIXELCLOCK_CMD_SET, &pixel_clock, sizeof(pixel_clock)));
	updateTimingInfo(TIMING_PIXEL_CLOCK);
}
void Camera::setFrameRate(double frame_rate)
{
	THROW_IF_ERROR(is_SetFrameRate(CameraHandle, frame_rate, &FrameRate));
	updateTimingInfo(TIMING_FRAME_RATE);
}
void Camera::setExposure(double exposure)
{
	THROW_IF_ERROR(is_Exposure(CameraHandle, IS_EXPOSURE_CMD_SET_EXPOSURE, &exposure, sizeof(exposure)));
	updateTimingInfo(TIMING_FRAME_RATE);
}


void Camera::imageCapture(ImageMemory &image_memory)
{
	THROW_IF_ERROR(is_SetImageMem(CameraHandle, image_memory.ptr(), image_memory.id()));
	THROW_IF_ERROR(is_FreezeVideo(CameraHandle, IS_WAIT));
}

void Camera::videoCaptureStart(std::vector<ImageMemory> &buffer)
{
	for(size_t i=0; i<buffer.size(); ++i)
	{
		THROW_IF_ERROR(is_AddToSequence(CameraHandle, buffer[i].ptr(), buffer[i].id()));
		SequencePtr[buffer[i].ptr()] = &(buffer[i]);
	}
	THROW_IF_ERROR(is_InitImageQueue(CameraHandle, 0));
	THROW_IF_ERROR(is_CaptureVideo(CameraHandle, IS_WAIT));
}

void Camera::videoCaptureStop()
{
	THROW_IF_ERROR(is_StopLiveVideo(CameraHandle, IS_WAIT));
	THROW_IF_ERROR(is_ExitImageQueue(CameraHandle));
	THROW_IF_ERROR(is_ClearSequence(CameraHandle));
	SequencePtr.clear();
}

ImageMemory* Camera::waitNextFrame(uint32_t timeout)
{
	char *ptr=NULL;
	int id;
	bool ok = false;
	while(!ok)
	{
		INT err = is_WaitForNextImage(CameraHandle, timeout, &ptr, &id);
		if(err == IS_CAPTURE_STATUS)
		{
			std::cout<<"missed frame"<<std::endl;
			if(ptr)
				THROW_IF_ERROR(is_UnlockSeqBuf(CameraHandle, id, ptr));
		}
		else
		{
			THROW_IF_ERROR(err);
			ok=true;
		}
	}
	if(ptr)
		return SequencePtr[ptr];
	else
		return NULL;
}

void Camera::unlockFrame(ImageMemory *frame)
{
	THROW_IF_ERROR(is_UnlockSeqBuf(CameraHandle, IS_IGNORE_PARAMETER, frame->ptr()));
}

HIDS Camera::handle()const
{
	return CameraHandle;
}

void Camera::updateTimingInfo(Camera::TimingUpdate update)
{
	THROW_IF_ERROR(is_Exposure(CameraHandle, IS_EXPOSURE_CMD_GET_EXPOSURE, &Exposure, sizeof(Exposure)));
	if(update == TIMING_EXPOSURE)
		return;
	
	double drange[3];
	THROW_IF_ERROR(is_Exposure(CameraHandle, IS_EXPOSURE_CMD_GET_EXPOSURE_RANGE, &drange, sizeof(drange)));
	ExposureRange = Range<double>(drange[0], drange[1], drange[2]);
	THROW_IF_ERROR(is_SetFrameRate(CameraHandle, IS_GET_FRAMERATE, &FrameRate));
	if(update == TIMING_FRAME_RATE)
		return;
	
	THROW_IF_ERROR(is_GetFrameTimeRange(CameraHandle, &drange[0], &drange[1], &drange[2]));
	FrameTimeRange = Range<double>(drange[0], drange[1], drange[2]);
	THROW_IF_ERROR(is_PixelClock(CameraHandle, IS_PIXELCLOCK_CMD_GET, &PixelClock, sizeof(PixelClock)));
	if(update == TIMING_PIXEL_CLOCK)
		return;
	
	UINT urange[3];
	THROW_IF_ERROR(is_PixelClock(CameraHandle, IS_PIXELCLOCK_CMD_GET_RANGE, urange, sizeof(urange)));
	PixelClockRange = Range<uint32_t>(urange[0], urange[1], urange[2]);
}

}


#include "ueye.hpp"
#include <iostream>

#include <opencv2/highgui/highgui.hpp>

int main(int argc, char **argv)
{
	is_SetErrorReport(0, IS_ENABLE_ERR_REP);
	ueye::Camera ueye_camera(0);
	
	std::vector<ueye::ImageMemory> buffer(2, ueye::ImageMemory(ueye_camera));
	ueye_camera.videoCaptureStart(buffer);
	ueye_camera.setPixelClock(ueye_camera.getPixelClockRange().max());
	ueye_camera.setFrameRate(1.0/ueye_camera.getFrameTimeRange().min());
	ueye_camera.setExposure(ueye_camera.getExposureRange().max());
	std::cout<<"Pixel clock : "<<ueye_camera.getPixelClock()<<std::endl;
	std::cout<<"FrameRate : "<<ueye_camera.getFrameRate()<<std::endl;
	std::cout<<"Exposure : "<<ueye_camera.getExposure()<<std::endl;
	cv::Mat mat;
	while(1)
	{
		ueye::ImageMemory *frame = ueye_camera.waitNextFrame();
		frame->copyToMat(mat);
		ueye_camera.unlockFrame(frame);
		cv::imshow("image", mat);
		char c=cv::waitKey(10);
		if(c == 27)
			break;
	}
	ueye_camera.videoCaptureStop();
	return 0;
}

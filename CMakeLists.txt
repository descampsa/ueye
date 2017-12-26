cmake_minimum_required (VERSION 2.6)
project (ueye_tool)

add_executable(ueye_capture_opencv ueye_capture_opencv.cpp ueye.cpp)
target_link_libraries(ueye_capture_opencv ueye_api opencv_core opencv_highgui)

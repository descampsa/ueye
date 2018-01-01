cmake_minimum_required (VERSION 3.1)
project (ueye_tool)
set (CMAKE_CXX_STANDARD 11)

add_executable(ueye_capture_opencv ueye_capture_opencv.cpp ueye.cpp)
target_link_libraries(ueye_capture_opencv ueye_api opencv_core opencv_highgui)

SET(WXWINDOWS_USE_GL 1)
find_package(wxWidgets COMPONENTS core base adv gl REQUIRED)
include(${wxWidgets_USE_FILE})

add_executable(ueye_gui ueye_gui.cpp ueye.cpp)
target_link_libraries(ueye_gui ${wxWidgets_LIBRARIES} ueye_api opencv_core GL)

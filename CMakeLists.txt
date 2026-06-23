cmake_minimum_required(VERSION 3.16)
project(Aurora_Remote_SDK_Demo)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
ENDIF()

MESSAGE("Build type: " ${CMAKE_BUILD_TYPE})


# Check C++14 support
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++14" COMPILER_SUPPORTS_CXX14)
if(COMPILER_SUPPORTS_CXX14)

else()
   if (NOT WIN32)
       message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support. Please use a different C++ compiler.")
   else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
   endif()
endif()


option(ENABLE_DEMOS_WITH_OPENCV "Build Demo with OpenCV" ON)
if(ENABLE_DEMOS_WITH_OPENCV)
    find_package(OpenCV 4.2 QUIET)
    if(OpenCV_FOUND)
        MESSAGE("OPENCV VERSION:")
        MESSAGE(${OpenCV_VERSION})
    else()
        MESSAGE(WARNING "OpenCV > 4.2 not found. Demos that use OpenCV will not be compiled.")
        set(ENABLE_DEMOS_WITH_OPENCV OFF)
    endif()
else()
    MESSAGE("OpenCV support is disabled.")
endif()




# Determine platform and architecture
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(AURORA_LIB_PATH "${PROJECT_SOURCE_DIR}/aurora_remote_public/lib/win64")
    else()
        message(FATAL_ERROR "Unsupported architecture for Windows. Only x64 is supported.")
    endif()
elseif(UNIX AND NOT APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(AURORA_LIB_PATH "${PROJECT_SOURCE_DIR}/aurora_remote_public/lib/linux_aarch64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        set(AURORA_LIB_PATH "${PROJECT_SOURCE_DIR}/aurora_remote_public/lib/linux_x86_64")
    else()
        message(FATAL_ERROR "Unsupported architecture for Linux. Only aarch64 and x86_64 are supported.")
    endif()
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(AURORA_LIB_PATH "${PROJECT_SOURCE_DIR}/aurora_remote_public/lib/macos_arm64")
    else()
        message(FATAL_ERROR "Unsupported architecture for macOS. Only arm64 is supported.")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform. Only Windows, Linux, and macOS are supported.")
endif()

# Set the library name
set(AURORA_LIB_NAME slamtec_aurora_remote_sdk)

# Add the determined library path to the linker search path
include_directories(${PROJECT_SOURCE_DIR}/aurora_remote_public/include)
link_directories(${AURORA_LIB_PATH})




message(STATUS "Aurora SDK library path: ${AURORA_LIB_PATH}")


find_package(Threads REQUIRED)

# ── Qt5 Aurora Dashboard ───────────────────────────────────────────────────
set(CMAKE_PREFIX_PATH "C:/Qt/Qt5.12.12/5.12.12/msvc2017_64" ${CMAKE_PREFIX_PATH})
find_package(Qt5 QUIET COMPONENTS Widgets OpenGL)

if(Qt5_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    add_executable(aurora_dashboard
        demo/aurora_dashboard/src/main.cpp
        demo/aurora_dashboard/src/MainWindow.cpp
        demo/aurora_dashboard/src/MainWindow.h
        demo/aurora_dashboard/src/MapWidget.cpp
        demo/aurora_dashboard/src/MapWidget.h
        demo/aurora_dashboard/src/LidarMapWidget.cpp
        demo/aurora_dashboard/src/LidarMapWidget.h
        demo/aurora_dashboard/src/SdkWorker.cpp
        demo/aurora_dashboard/src/SdkWorker.h
        demo/aurora_dashboard/src/CameraPreviewWidget.cpp
        demo/aurora_dashboard/src/CameraPreviewWidget.h
        demo/aurora_dashboard/src/DepthCamWidget.cpp
        demo/aurora_dashboard/src/DepthCamWidget.h
        demo/aurora_dashboard/src/SemanticSegmentationWidget.cpp
        demo/aurora_dashboard/src/SemanticSegmentationWidget.h
        demo/aurora_dashboard/src/NavigationGuidance.h
        demo/aurora_dashboard/src/NavigationWidget.cpp
        demo/aurora_dashboard/src/NavigationWidget.h
    )
    target_link_libraries(aurora_dashboard
        ${AURORA_LIB_NAME}
        Qt5::Widgets
        Qt5::Core
        Qt5::Gui
        Qt5::OpenGL
    )

    # Static Qt 依賴庫
    target_link_libraries(aurora_dashboard
        ws2_32 ole32 oleaut32 imm32 winmm
    )
    set_target_properties(aurora_dashboard PROPERTIES
        WIN32_EXECUTABLE TRUE
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON
    )
    if(MSVC)
        target_compile_options(aurora_dashboard PRIVATE /utf-8)
    endif()
    add_custom_command(TARGET aurora_dashboard POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${AURORA_LIB_PATH}
        $<TARGET_FILE_DIR:aurora_dashboard>)
    add_custom_command(TARGET aurora_dashboard POST_BUILD
        COMMAND "C:/Qt/Qt5.12.12/5.12.12/msvc2017_64/bin/windeployqt.exe"
        $<TARGET_FILE:aurora_dashboard>)
    message(STATUS "Qt5 found: aurora_dashboard target added")
else()
    message(WARNING "Qt5 not found, aurora_dashboard will not be built")
endif()

# Build Demos

## simple_pose
add_executable(simple_pose demo/simple_pose/src/simple_pose.cpp)
target_link_libraries(simple_pose ${AURORA_LIB_NAME})
# copy the .so or .dll with ${AURORA_LIB_NAME} to the build directory
add_custom_command(TARGET simple_pose POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory  
    ${AURORA_LIB_PATH}
    $<TARGET_FILE_DIR:simple_pose>)

## relocalization
add_executable(relocalization demo/relocalization/src/relocalization.cpp)
target_link_libraries(relocalization ${AURORA_LIB_NAME})
# copy the .so or .dll with ${AURORA_LIB_NAME} to the build directory
add_custom_command(TARGET relocalization POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory  
    ${AURORA_LIB_PATH}
    $<TARGET_FILE_DIR:relocalization>)

## pure_c_demo
add_executable(pure_c_demo demo/pure_c_demo/src/pure_c_demo.c)
target_link_libraries(pure_c_demo ${AURORA_LIB_NAME})


## imu_fetcher
add_executable(imu_fetcher demo/imu_fetcher/src/imu_fetcher.cpp)
target_link_libraries(imu_fetcher ${AURORA_LIB_NAME})


## vslam_map_saveload
add_executable(vslam_map_saveload demo/vslam_map_saveload/src/vslam_map_saveload.cpp)
target_link_libraries(vslam_map_saveload ${AURORA_LIB_NAME})

## device_info_monitor
add_executable(device_info_monitor demo/device_info_monitor/src/device_info_monitor.cpp)
target_link_libraries(device_info_monitor ${AURORA_LIB_NAME})

## colmap_recorder
add_executable(colmap_recorder demo/colmap_recorder/src/colmap_recorder.cpp)
target_link_libraries(colmap_recorder ${AURORA_LIB_NAME})

## time_sync
add_executable(time_sync demo/time_sync/src/time_sync.cpp)
target_link_libraries(time_sync ${AURORA_LIB_NAME})

## pose_augmentation
add_executable(pose_augmentation demo/pose_augmentation/src/pose_augmentation.cpp)
target_link_libraries(pose_augmentation ${AURORA_LIB_NAME})

## pose_covariance
add_executable(pose_covariance demo/pose_covariance/src/pose_covariance.cpp)
target_link_libraries(pose_covariance ${AURORA_LIB_NAME})

## persistent_config
add_executable(persistent_config demo/persistent_config/src/persistent_config.cpp)
target_link_libraries(persistent_config ${AURORA_LIB_NAME})

## transform_manager
add_executable(transform_manager demo/transform_manager/src/transform_manager.cpp)
target_link_libraries(transform_manager ${AURORA_LIB_NAME})

## dashcam_recorder
add_executable(dashcam_recorder demo/dashcam_recorder/src/dashcam_recorder.cpp)
target_link_libraries(dashcam_recorder ${AURORA_LIB_NAME})

## system_power
add_executable(system_power demo/system_power/src/system_power.cpp)
target_link_libraries(system_power ${AURORA_LIB_NAME})

## frame preview, only enabled when ENABLE_DEMOS_WITH_OPENCV is ON
if(ENABLE_DEMOS_WITH_OPENCV)

## frame preview
add_executable(frame_preview demo/frame_preview/src/frame_preview.cpp)
target_include_directories(frame_preview PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(frame_preview ${AURORA_LIB_NAME} ${OpenCV_LIBS})

## map render
add_executable(map_render demo/map_render/src/map_render.cpp)
target_include_directories(map_render PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(map_render ${AURORA_LIB_NAME}  ${OpenCV_LIBS})
    
## lidar scan plot
add_executable(lidar_scan_plot demo/lidar_scan_plot/src/lidar_scan_plot.cpp)
target_include_directories(lidar_scan_plot PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(lidar_scan_plot ${AURORA_LIB_NAME}  ${OpenCV_LIBS})

## lidar 2d map render
add_executable(lidar_2dmap_render demo/lidar_2dmap_render/src/lidar_2dmap_render.cpp)
target_include_directories(lidar_2dmap_render PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(lidar_2dmap_render ${AURORA_LIB_NAME}  ${OpenCV_LIBS})

## depth camera view
add_executable(depthcam_view demo/depthcam_view/src/depthcam_view.cpp)
target_include_directories(depthcam_view PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(depthcam_view ${AURORA_LIB_NAME}  ${OpenCV_LIBS} Threads::Threads)

## semantic segmentation
add_executable(semantic_segmentation demo/semantic_segmentation/src/semantic_segmentation.cpp)
target_include_directories(semantic_segmentation PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(semantic_segmentation ${AURORA_LIB_NAME}  ${OpenCV_LIBS})

## calibration exporter
add_executable(calibration_exporter demo/calibration_exporter/src/calibration_exporter.cpp)
target_include_directories(calibration_exporter PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(calibration_exporter ${AURORA_LIB_NAME}  ${OpenCV_LIBS})

## camera_mask (with OpenCV support for image I/O)
add_executable(camera_mask demo/camera_mask/src/camera_mask.cpp)
target_include_directories(camera_mask PRIVATE ${OpenCV_INCLUDE_DIRS})
target_compile_definitions(camera_mask PRIVATE AURORA_SDK_HAS_OPENCV)
target_link_libraries(camera_mask ${AURORA_LIB_NAME}  ${OpenCV_LIBS})

endif()

## Aurora diagnostic tool (no OpenCV dependency)
add_executable(aurora_diagnostic demo/aurora_diagnostic/src/aurora_diagnostic.cpp)
target_link_libraries(aurora_diagnostic ${AURORA_LIB_NAME})

# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.10.2)

project(vins_mono)

add_subdirectory(camera_model)
add_subdirectory(feature_tracker)
add_subdirectory(vins_estimator)

add_library( # Sets the name of the library.
        ${PROJECT_NAME}
        # Sets the library as a shared library.
        SHARED
        # Provides a relative path to your source file(s).
        android/algorithm_interface.cpp
        android/ob_system.cpp
        system.cpp
        )

target_link_libraries(${PROJECT_NAME} # Specifies the target library.
        platform-lib
        camera_model
        feature_tracker
        vins_estimator
#        ${jniLibs_DIR}/libqspower-1.4.so
        )

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/lib-includes
        ${CMAKE_SOURCE_DIR}/platform
        ${CMAKE_SOURCE_DIR}/platform/rosbag/include)
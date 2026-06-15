cmake_minimum_required(VERSION 3.15)
project(screen-vision-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point CMake at your OpenCV build, e.g.:
#   cmake -S . -B build -DOpenCV_DIR=C:/opencv/build
find_package(OpenCV REQUIRED)

add_library(svc STATIC
    src/color_vision.cpp
    src/window_finder.cpp
    src/window_capture.cpp
    src/input_controller.cpp
    src/timer.cpp
    src/logger.cpp
)
target_include_directories(svc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(svc PUBLIC
    ${OpenCV_LIBS}
    user32   # Win32 input + window queries
    gdi32    # GDI capture (BitBlt, etc.)
)

add_executable(bar_reader examples/bar_reader.cpp)
target_link_libraries(bar_reader PRIVATE svc)

# Copy the OpenCV runtime DLLs next to the demo executable on Windows.
if(WIN32 AND OpenCV_FOUND)
    add_custom_command(TARGET bar_reader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:bar_reader>
        $<TARGET_FILE_DIR:bar_reader>
        COMMAND_EXPAND_LISTS
    )
endif()

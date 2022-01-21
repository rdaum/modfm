cmake_minimum_required(VERSION 3.21)
include("cmake/HunterGate.cmake")
HunterGate(
        URL "https://github.com/cpp-pm/hunter/archive/v0.23.297.tar.gz"
        SHA1 "3319fe6a3b08090df7df98dee75134d68e2ef5a3"
)
project(modfm)

hunter_add_package(gflags)
find_package(gflags CONFIG REQUIRED)

hunter_add_package(glog)
find_package(glog CONFIG REQUIRED)

hunter_add_package(GTest)
find_package(GTest CONFIG REQUIRED)


# Add our local cmake dir.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)


set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(IMGUI STATIC)

target_sources(IMGUI
        PRIVATE
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui.cpp

        PRIVATE
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp
        )

target_include_directories(IMGUI
        PUBLIC ${IMGUI_DIR}
        PUBLIC ${IMGUI_DIR}/backends
        )
find_package(OpenGL REQUIRED)
target_link_libraries(IMGUI PUBLIC ${OPENGL_LIBRARIES})


add_executable(modfm src/main.cc
        src/gui.cc
        src/oscillator.h
        src/oscillator.cc
        src/player.h
        src/player.cc
        src/patch.h
        src/envgen.h
        src/envgen.cc)

add_subdirectory(third_party/abseil-cpp)
include_directories(${IMGUI_DIR})
add_subdirectory(third_party/imgui-plot)
set(KISSFFT_TOOLS OFF)
set(KISSFFT_TEST OFF)
set(KISSFFT_DATATYPE float)
add_subdirectory(third_party/kissfft)
add_subdirectory(third_party/portaudio)
add_subdirectory(third_party/portmidi)

target_include_directories(modfm PRIVATE
        third_party/imgui-plot/include
        third_party/kissfft
        third_party/portaudio/include
        third_party/portmidi/porttime
        third_party/portmidi/pm_common)
if (WIN32)
    set(PLATFORM_LIBRARIES})
else ()
    set(PLATFORM_LIBRARIES pthread rt GL glfw)
endif ()

target_link_libraries(modfm
        ${PLATFORM_LIBRARIES}
        IMGUI
        glog::glog
        gflags
        glfw
        kissfft
        tbb
        pulse-simple
        imgui_plot
        PortAudio
        portmidi
        absl::statusor)

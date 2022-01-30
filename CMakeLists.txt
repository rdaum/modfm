cmake_minimum_required(VERSION 3.21)
include("cmake/HunterGate.cmake")
HunterGate(
        URL "https://github.com/cpp-pm/hunter/archive/v0.23.297.tar.gz"
        SHA1 "3319fe6a3b08090df7df98dee75134d68e2ef5a3"
)
project(modfm)

include(FetchContent)
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

add_executable(modfm src/main.cc
        src/oscillator.h
        src/oscillator.cc
        src/player.h
        src/player.cc
        src/patch.h
        src/patch.cc
        src/midi.h
        src/midi.cc
        src/envgen.h
        src/ui/patch_editor.h
        src/ui/patch_editor.cc
        src/ui/drawbars.h
        src/ui/drawbars.cc
        src/ui/device_selector.h
        src/ui/device_selector.cc
        src/ui/gui.h
        src/ui/gui.cc
        src/envgen.cc)

set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)

FetchContent_Declare(
        nanogui
        GIT_REPOSITORY https://github.com/mitsuba-renderer/nanogui.git
)
FetchContent_MakeAvailable(nanogui)

add_subdirectory(third_party/abseil-cpp)
add_subdirectory(third_party/portaudio)
add_subdirectory(third_party/portmidi)
add_subdirectory(third_party/sigslot)


target_include_directories(modfm PRIVATE
        src/
        third_party/sigslot
        third_party/portaudio/include
        third_party/portmidi/porttime
        third_party/portmidi/pm_common)
if (WIN32)
    set(PLATFORM_LIBRARIES})
else ()
    set(PLATFORM_LIBRARIES pthread rt GL glfw tbb)
endif ()

target_link_libraries(modfm
        ${PLATFORM_LIBRARIES}
        glog::glog
        gflags
        glfw
        pulse-simple
        PortAudio
        Pal::Sigslot
        portmidi
        nanogui
        absl::statusor)

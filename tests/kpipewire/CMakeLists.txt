cmake_minimum_required(VERSION 3.25)
project(NkShotTimelinePauseChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(Qt6 REQUIRED COMPONENTS Core Gui Test QmlIntegration)
find_package(Qt6GuiPrivate REQUIRED)
find_package(KF6CoreAddons REQUIRED)
find_package(KPipeWire REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCodec REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUtil REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(AVFormat REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVFilter REQUIRED IMPORTED_TARGET libavfilter)
pkg_check_modules(GBM REQUIRED IMPORTED_TARGET gbm)
pkg_check_modules(SWScale REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules(LIBVA REQUIRED IMPORTED_TARGET libva)
pkg_check_modules(LIBVA_DRM REQUIRED IMPORTED_TARGET libva-drm)
pkg_check_modules(PipeWire REQUIRED IMPORTED_TARGET libpipewire-0.3)
find_package(epoxy REQUIRED)
find_package(Libdrm REQUIRED)
if(NOT KPIPEWIRE_SOURCE OR NOT KPIPEWIRE_BINARY)
 message(FATAL_ERROR "Pass patched KPIPEWIRE_SOURCE and its configured KPIPEWIRE_BINARY")
endif()
set(S ${KPIPEWIRE_SOURCE}/src)
set(B ${KPIPEWIRE_BINARY}/src)
add_executable(timeline_pause_test TimelinePauseTest.cpp
 ${S}/pipewireproduce.cpp ${S}/pipewirebaseencodedstream.cpp ${S}/pipewirerecord.cpp
 ${S}/encoder.cpp ${S}/audioencoder.cpp ${S}/aacencoder.cpp ${S}/libopusencoder.cpp
 ${S}/gifencoder.cpp ${S}/h264vaapiencoder.cpp ${S}/libx264encoder.cpp
 ${S}/libopenh264encoder.cpp ${S}/libvpxencoder.cpp ${S}/libvpxvp9encoder.cpp ${S}/libwebpencoder.cpp
 ${S}/vaapiutils.cpp ${S}/rendernodecontext.cpp
 ${B}/logging_record.cpp ${B}/logging_frame_statistics.cpp ${B}/logging_libav.cpp ${B}/logging_vaapi.cpp)
target_include_directories(timeline_pause_test PRIVATE ${S} ${B})
target_compile_definitions(timeline_pause_test PRIVATE MESA_EGL_NO_X11_HEADERS EGL_NO_X11 EGL_NO_PLATFORM_SPECIFIC_TYPES)
target_link_libraries(timeline_pause_test PRIVATE Qt6::Test Qt6::Gui Qt6::GuiPrivate Qt6::QmlIntegration KF6::CoreAddons
 K::KPipeWire K::KPipeWireDmaBuf PkgConfig::PipeWire PkgConfig::AVCodec PkgConfig::AVUtil PkgConfig::AVFormat PkgConfig::AVFilter
 PkgConfig::GBM PkgConfig::SWScale PkgConfig::LIBVA PkgConfig::LIBVA_DRM epoxy::epoxy Libdrm::Libdrm)
enable_testing()
add_test(NAME timeline_pause_test COMMAND timeline_pause_test)
set_tests_properties(timeline_pause_test PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

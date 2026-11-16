# SPDX-FileCopyrightText: 2020 Carl Schwan <carl@carlschwan.eu>
#
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.16)

set(REQUIRED_KF_VERSION 6.13.0)
set(REQUIRED_QT_VERSION 6.8.0)

# Overriding this with `-DREQUIRED_HWY_VERSION=…` is not officially supported.
# If your distro only provides older versions, you can try to override this and
# see if the project still works fine.
set(REQUIRED_HWY_VERSION 1.3.0)

project(KQuickImageEditor LANGUAGES CXX VERSION 0.7.0)

# NkShot vendoring delta: link this module statically into the application.
set(BUILD_SHARED_LIBS OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${REQUIRED_KF_VERSION} NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(FeatureSummary)
include(CMakePackageConfigHelpers)
include(CMakeDependentOption)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(KDEClangFormat)
include(KDEGitCommitHooks)
include(KDEPackageAppTemplates)

include(ECMAddTests)
include(ECMSetupVersion)
include(ECMGenerateExportHeader)
include(ECMGenerateHeaders)
include(ECMQmlModule)
include(ECMGenerateQDoc)

find_package(Qt6 ${REQUIRED_QT_VERSION} COMPONENTS Core Qml Quick Concurrent ShaderTools REQUIRED)
find_package(KF6 ${REQUIRED_KF_VERSION} REQUIRED Config)
find_package(HWY ${REQUIRED_HWY_VERSION} REQUIRED)

set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KQuickImageEditor")
set(KQuickImageEditor_INSTALL_INCLUDEDIR ${KDE_INSTALL_INCLUDEDIR}/KQuickImageEditor)
set(kquickimageeditor_INSTALL_INCLUDEDIR ${KDE_INSTALL_INCLUDEDIR}/kquickimageeditor)

ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX KQUICKIMAGEEDITOR
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kquickimageeditor_version.h"
    PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KQuickImageEditorConfigVersion.cmake"
    SOVERSION 1
)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/KQuickImageEditorConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/KQuickImageEditorConfig.cmake"
    INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
    PATH_VARS CMAKE_INSTALL_PREFIX
)

add_subdirectory(src)
# NkShot vendoring delta: build the module statically so it is linked into the
# NkShot executable and resolved through qt_import_qml_plugins(), exactly like
# the pinned Kos.Ui module. A shared build would need a runtime QML import path.
# if (BUILD_TESTING)
#   add_subdirectory(autotests)
# endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kquickimageeditor_version.h DESTINATION ${KQuickImageEditor_INSTALL_INCLUDEDIR} COMPONENT Devel)
# NkShot vendoring delta: no exported CMake package. A statically linked module
# generates resource object targets that cannot be placed in an export set, and
# nothing consumes KQuickImageEditorTargets inside NkShot.

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

# NkShot vendoring delta: the clang-format target and the git pre-commit hook are
# dropped here. NkShot owns those names for its own tree, and a vendored subtree
# must never install hooks into the enclosing repository.


# rsgain CMakeLists.txt
# Copyright (C) 2014 Alessandro Ghedini <alessandro@ghedini.me>
# Modifications Copyright (C) 2019 Matthias C. Hormann <mhormann@gmx.de>
# rsgain by complexlogic, 2022
# This file is released under the 2 clause BSD license, see COPYING

cmake_minimum_required(VERSION 3.13)
project(rsgain 
  VERSION 3.0
  DESCRIPTION "ReplayGain 2.0 loudness normalizer" 
  HOMEPAGE_URL "https://github.com/complexlogic/rsgain"
  LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD 20)
set(EXECUTABLE_TITLE "rsgain")
include_directories(${PROJECT_BINARY_DIR})

# GCC 9 and earlier are not supported due to C++20 features
if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10)
  message(FATAL_ERROR "GCC 10 and later supported only")
endif ()

# Set Visual Studio startup project
if (WIN32)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${EXECUTABLE_TITLE})
endif ()

configure_file("${PROJECT_SOURCE_DIR}/config/config.h.in" "${PROJECT_BINARY_DIR}/config.h")

# Find dependencies - Windows
if (WIN32)
  find_package(ffmpeg MODULE REQUIRED)
  find_path(TAGLIB_INCLUDE_DIR "taglib/id3v2tag.h" REQUIRED)
  set(TAGLIB_INCLUDE_DIR ${TAGLIB_INCLUDE_DIR}/taglib)
  find_library(TAGLIB tag REQUIRED)
  find_path(LIBEBUR128_INCLUDE_DIR "ebur128.h" REQUIRED)
  find_library(LIBEBUR128 ebur128 REQUIRED)
  find_path(FMT_INCLUDE_DIR "fmt/core.h" REQUIRED)
  find_library(FMT fmt REQUIRED)
  find_path(GETOPT_INCLUDE_DIR "getopt.h" REQUIRED)
  find_library(GETOPT getopt REQUIRED)
  find_path(INIH_INCLUDE_DIR "ini.h" REQUIRED)
  find_library(INIH inih REQUIRED)
  
# Find dependencies - Linux/Mac
elseif (UNIX)
  if (APPLE)
    # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
    link_directories(/usr/local/lib)
  endif (APPLE)
  
  find_package(PkgConfig MODULE REQUIRED)
  pkg_check_modules(LIBAVCODEC REQUIRED IMPORTED_TARGET libavcodec)
  pkg_check_modules(LIBAVFORMAT REQUIRED IMPORTED_TARGET libavformat)
  pkg_check_modules(LIBSWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)
  pkg_check_modules(LIBAVUTIL REQUIRED IMPORTED_TARGET libavutil)
  pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib)
  pkg_check_modules(LIBEBUR128 REQUIRED IMPORTED_TARGET libebur128>=1.2.4)
  pkg_check_modules(INIH REQUIRED IMPORTED_TARGET inih)
  pkg_check_modules(FMT REQUIRED IMPORTED_TARGET fmt)
endif()

# Generate Windows application manifest and resource file
if (WIN32)
  set(VERSION_M ${PROJECT_VERSION_MAJOR})
  if (PROJECT_VERSION_MINOR)
    set(VERSION_N ${PROJECT_VERSION_MINOR})
  else ()
    set(VERSION_N 0)
  endif()
  if (PROJECT_VERSION_PATCH)
    set(VERSION_O ${PROJECT_VERSION_PATCH})
  else ()
    set(VERSION_O 0)
  endif()
  if (PROJECT_VERSION_TWEAK)
    set(VERSION_P ${PROJECT_VERSION_TWEAK})
  else ()
    set(VERSION_P 0)
  endif()
  configure_file(${PROJECT_SOURCE_DIR}/config/${PROJECT_NAME}.manifest.in ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.manifest)
  configure_file(${PROJECT_SOURCE_DIR}/config/versioninfo.rc.in ${PROJECT_BINARY_DIR}/versioninfo.rc)
endif()

# Build source files
add_subdirectory(src)

# Installation - Windows
if (WIN32)
  #set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)
  #install(TARGETS ${EXECUTABLE_TITLE} DESTINATION ".")
  install(DIRECTORY ${PROJECT_BINARY_DIR}/$<CONFIG>/ DESTINATION ".")
  configure_file(${PROJECT_SOURCE_DIR}/CHANGELOG ${PROJECT_BINARY_DIR}/CHANGELOG.txt)
  install(FILES ${PROJECT_BINARY_DIR}/CHANGELOG.txt DESTINATION ".")
  configure_file(${PROJECT_SOURCE_DIR}/LICENSE ${PROJECT_BINARY_DIR}/LICENSE.txt)
  install(FILES ${PROJECT_BINARY_DIR}/LICENSE.txt DESTINATION ".")
  configure_file(${PROJECT_SOURCE_DIR}/config/overrides.ini ${PROJECT_BINARY_DIR}/overrides.ini)
  install(FILES ${PROJECT_BINARY_DIR}/overrides.ini DESTINATION ".")
  
  # Copy the VC Runtime DLLs in case user doesn't have them installed
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
  include(InstallRequiredSystemLibraries)
  set(required_libs "vcruntime140.dll;vcruntime140_1.dll;msvcp140.dll")
  foreach(required_lib ${required_libs})
    foreach(system_lib ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
      string(FIND ${system_lib} ${required_lib} vcruntime)
      if (NOT vcruntime EQUAL -1)
        install(FILES ${system_lib} DESTINATION ".")
      endif ()
    endforeach ()
  endforeach ()
  
  # Set up CPack
  set(CPACK_PACKAGE_NAME ${EXECUTABLE_TITLE})
  set(CPACK_GENERATOR "ZIP")
  include(CPack)
  
# Installation - Linux/Mac
elseif (UNIX)
  install(TARGETS ${EXECUTABLE_TITLE} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
  install(FILES ${PROJECT_SOURCE_DIR}/config/overrides.ini DESTINATION ${CMAKE_INSTALL_PREFIX}/share/${EXECUTABLE_TITLE})
  
  # Build Debian packages
  if (PACKAGE STREQUAL "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME ${EXECUTABLE_TITLE})
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
    set(CPACK_DEBIAN_PACKAGE_VERSION ${CMAKE_PROJECT_VERSION})
    if (NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
      set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    endif()
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libavcodec58 (>= 4.2.4), libavutil56 (>= 4.2.4), libswresample3  (>= 4.2.4), libavformat58 (>= 4.2.4), libtag1v5 (>= 1.11.1), libebur128-1 (>=1.2.4), fmtlib, libinih1, libc6 (>=2.29), libstdc++6 (>=10.2)")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "complexlogic")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_ARCHIVE_TYPE "gnutar")
    set(CPACK_DEBIAN_COMPRESSION_TYPE "gzip")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_PACKAGE_NAME ${EXECUTABLE_TITLE})
    set(CPACK_GENERATOR ${PACKAGE})
    include(CPack)
  endif ()

  # Provide 'uninstall' target
  if(NOT TARGET uninstall)
      configure_file(
          "${PROJECT_SOURCE_DIR}/config/cmake_uninstall.cmake.in"
          "${PROJECT_BINARY_DIR}/cmake_uninstall.cmake"
          IMMEDIATE @ONLY)

      add_custom_target(uninstall
          COMMAND ${CMAKE_COMMAND} -P ${PROJECT_BINARY_DIR}/cmake_uninstall.cmake)
  endif()
endif()




# loudgain CMakeLists.txt
# Copyright (C) 2014 Alessandro Ghedini <alessandro@ghedini.me>
# Modifications Copyright (C) 2019 Matthias C. Hormann <mhormann@gmx.de>
# Windows port by complexlogic, 2022
# This file is released under the 2 clause BSD license, see COPYING

cmake_minimum_required(VERSION 3.13)
project(
  loudgain 
  VERSION 1.0.1
  DESCRIPTION "ReplayGain 2.0 loudness normalizer" 
  HOMEPAGE_URL "https://github.com/complexlogic/loudgain"
  LANGUAGES C CXX
)
set(EXECUTABLE_TITLE "loudgain")
configure_file(
  "${PROJECT_SOURCE_DIR}/config/config.h.in" 
  "${PROJECT_BINARY_DIR}/config.h"
)

# Find dependencies - Windows
if (WIN32)
  find_package(ffmpeg MODULE REQUIRED)
  
  find_path(TAGLIB_INCLUDE_DIR "taglib/id3v2tag.h" REQUIRED)
  set(TAGLIB_INCLUDE_DIR ${TAGLIB_INCLUDE_DIR}/taglib)
  find_library(TAGLIB tag REQUIRED)
  
  find_path(LIBEBUR128_INCLUDE_DIR "ebur128.h" REQUIRED)
  find_library(LIBEBUR128 ebur128 REQUIRED)
  
  find_path(GETOPT_INCLUDE_DIR "getopt.h" REQUIRED)
  find_library(GETOPT getopt REQUIRED)
  
  include_directories(
    ${FFMPEG_INCLUDE_DIRS} 
    ${TAGLIB_INCLUDE_DIR} 
    ${LIBEBUR128_INCLUDE_DIR} 
    ${GETOPT_INCLUDE_DIR}
    ${PROJECT_BINARY_DIR}
  )
  link_libraries(
    ${FFMPEG_LIBRARIES} 
    ${TAGLIB} 
    ${LIBEBUR128} 
    ${GETOPT}
  )
  
# Find dependencies - Linux/Mac
else ()
  if (APPLE)
    # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
    link_directories(/usr/local/lib)
  endif (APPLE)
  
  find_package(PkgConfig MODULE REQUIRED)
  pkg_check_modules(LAVC REQUIRED libavcodec)
  pkg_check_modules(LAVF REQUIRED libavformat)
  if (${LAVF_VERSION} VERSION_LESS 58.9.100)
    add_compile_definitions(LAVF_REGISTER_ALL=1)
  endif ()
  pkg_check_modules(LAVR REQUIRED libswresample)
  pkg_check_modules(LAVU REQUIRED libavutil)
  pkg_check_modules(LTAG REQUIRED taglib)
  pkg_check_modules(EBUR128 REQUIRED libebur128>=1.2.4)

  include_directories(
    ${LAVC_INCLUDE_DIRS}
    ${LAVF_INCLUDE_DIRS}
    ${LAVR_INCLUDE_DIRS}
    ${LAVU_INCLUDE_DIRS}
    ${LTAG_INCLUDE_DIRS}
    ${EBUR128_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
  )
  link_libraries(
    ${LAVC_LIBRARIES}
    ${LAVF_LIBRARIES}
    ${LAVR_LIBRARIES}
    ${LAVU_LIBRARIES}
    ${LTAG_LIBRARIES}
    ${EBUR128_LIBRARIES}
  )
endif()

# Generate Windows application manifest
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
  configure_file(
    ${PROJECT_SOURCE_DIR}/config/${PROJECT_NAME}.manifest.in 
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.manifest
  )
endif()

# Build source files
add_subdirectory(src)

# Compile options - Linux/Mac
if (UNIX)
  set_target_properties(loudgain PROPERTIES COMPILE_FLAGS "-Wall -pedantic -g")
  set(CMAKE_C_FLAGS "-std=gnu99 -D_GNU_SOURCE")
  set(CMAKE_CXX_FLAGS "-std=gnu++11 -D_GNU_SOURCE")
endif ()

# Installation - Windows
if (WIN32)
  install(DIRECTORY ${PROJECT_BINARY_DIR}/Release/ DESTINATION "./")
  install(PROGRAMS ${PROJECT_SOURCE_DIR}/scripts/loudgain-scanner.py DESTINATION "./")
  install(FILES "${PROJECT_SOURCE_DIR}/LICENSE.txt" DESTINATION "./")
  install(FILES "${PROJECT_SOURCE_DIR}/CHANGELOG.txt" DESTINATION "./")
  
  # Copy the VC Runtime DLLs in case user doesn't have them installed
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
  include(InstallRequiredSystemLibraries)
  set(required_libs "vcruntime140.dll;vcruntime140_1.dll;msvcp140.dll")
  foreach(required_lib ${required_libs})
    foreach(system_lib ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
      string(FIND ${system_lib} ${required_lib} vcruntime)
      if (NOT vcruntime EQUAL -1)
        install(FILES ${system_lib} DESTINATION "./")
      endif ()
    endforeach ()
  endforeach ()
  
  # Set up CPack
  set(CPACK_PACKAGE_NAME ${EXECUTABLE_TITLE})
  set(CPACK_GENERATOR "ZIP")
  include(CPack)
  
# Installation - Linux/Mac
else ()
  install(TARGETS ${EXECUTABLE_TITLE} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
  install(PROGRAMS ${PROJECT_SOURCE_DIR}/scripts/loudgain-scanner.py DESTINATION ${CMAKE_INSTALL_PREFIX}/bin RENAME "loudgain-scanner")
  install(FILES
    ${PROJECT_SOURCE_DIR}/docs/loudgain.1
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1
  )
  
  # Build Debian packages
  if (PACKAGE STREQUAL "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME ${EXECUTABLE_TITLE})
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
    set(CPACK_DEBIAN_PACKAGE_VERSION ${CMAKE_PROJECT_VERSION})
    if (NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
      set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    endif()
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libavcodec58 (>= 4.2.4), libavutil56 (>= 4.2.4), libswresample3  (>= 4.2.4), libavformat58 (>= 4.2.4), libtag1v5 (>= 1.11.1), libebur128-1 (>=1.2.4), libc6 (>=2.29)")
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




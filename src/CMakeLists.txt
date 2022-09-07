set(SOURCE_FILES "rsgain.cpp" "scan.cpp" "output.cpp" "tag.cpp" "easymode.cpp")
if (WIN32)
  add_executable(${EXECUTABLE_TITLE} ${SOURCE_FILES} "${PROJECT_BINARY_DIR}/rsgain.manifest" "${PROJECT_BINARY_DIR}/versioninfo.rc")
  target_compile_options(${EXECUTABLE_TITLE} PUBLIC "/Zc:preprocessor")
  target_include_directories(${EXECUTABLE_TITLE} PUBLIC
    ${FFMPEG_INCLUDE_DIR} 
    ${TAGLIB_INCLUDE_DIR} 
    ${LIBEBUR128_INCLUDE_DIR}
    ${FMT_INCLUDE_DIR}
    ${GETOPT_INCLUDE_DIR}
    ${INIH_INCLUDE_DIR} 
  )
  target_link_libraries(${EXECUTABLE_TITLE}
    ${LIBAVFORMAT}
	${LIBAVCODEC}
	${LIBAVUTIL}
	${LIBSWRESAMPLE}
    ${TAGLIB} 
    ${LIBEBUR128} 
    ${FMT}
    ${GETOPT}
    ${INIH}
  )
  if (VCPKG_TARGET_TRIPLET STREQUAL "custom-triplet")
    target_link_libraries(${EXECUTABLE_TITLE} ${STATIC_LIBS})
  endif ()
  
elseif (UNIX)
  add_executable(${EXECUTABLE_TITLE} ${SOURCE_FILES})
  target_link_libraries(${EXECUTABLE_TITLE}
    PkgConfig::LIBAVFORMAT
    PkgConfig::LIBAVCODEC
    PkgConfig::LIBSWRESAMPLE
    PkgConfig::LIBAVUTIL
    PkgConfig::TAGLIB
    PkgConfig::LIBEBUR128
    PkgConfig::INIH
    PkgConfig::FMT
  )

  # GCC 10 requires linking with pthread for some reason
  if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
    target_link_libraries(${EXECUTABLE_TITLE} pthread)
  endif ()
endif()
set (EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")
set(SOURCE_FILES 
  rsgain.cpp
  rsgain.hpp
  scan.cpp
  scan.hpp
  output.cpp
  output.hpp
  tag.cpp
  tag.hpp
  easymode.cpp
  easymode.hpp
)
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
    ${GETOPT}
    ${INIH}
    FDK-AAC::fdk-aac
  )
  if (VCPKG_TARGET_TRIPLET STREQUAL "custom-triplet")
    target_link_libraries(${EXECUTABLE_TITLE} ${STATIC_LIBS})
  endif ()
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

elseif (UNIX)
  add_executable(${EXECUTABLE_TITLE} ${SOURCE_FILES})
  if(NOT UCHECKMARKS)
    target_compile_definitions(${EXECUTABLE_TITLE} PUBLIC "NOUCHECKMARKS")
  endif()
  target_link_libraries(${EXECUTABLE_TITLE}
    PkgConfig::LIBAVFORMAT
    PkgConfig::LIBAVCODEC
    PkgConfig::LIBSWRESAMPLE
    PkgConfig::LIBAVUTIL
    PkgConfig::TAGLIB
    PkgConfig::LIBEBUR128
    PkgConfig::INIH
    Threads::Threads
  )
  if (NOT USE_STD_FORMAT)
    target_link_libraries(${EXECUTABLE_TITLE} PkgConfig::FMT)
  endif ()
  if (STRIP)
    add_custom_command(TARGET ${EXECUTABLE_TITLE}
      POST_BUILD
      COMMAND "${STRIP}" "${PROJECT_BINARY_DIR}/${EXECUTABLE_TITLE}"
    )
  endif ()
  if (INSTALL_MANPAGE)
    add_custom_command(TARGET ${EXECUTABLE_TITLE}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/docs/${EXECUTABLE_TITLE}.1" "${PROJECT_BINARY_DIR}"
      COMMAND "${GZIP}" -f "${PROJECT_BINARY_DIR}/${EXECUTABLE_TITLE}.1"
    )
  endif ()
endif()
set (EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")
string(TIMESTAMP BUILD_DATE "%Y-%m-%d")
add_compile_definitions("BUILD_DATE=\"${BUILD_DATE}\"")
if (MAXPROGBARWIDTH GREATER_EQUAL 20)
  target_compile_definitions(${EXECUTABLE_TITLE} PUBLIC "MAXPROGBARWIDTH=${MAXPROGBARWIDTH}")
endif ()

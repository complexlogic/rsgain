if (WIN32)
  add_executable(${EXECUTABLE_TITLE} "rsgain.c" "scan.cpp" "output.c" "tag.cpp" "easymode.cpp" "${PROJECT_BINARY_DIR}/rsgain.manifest")
else()
  add_executable(${EXECUTABLE_TITLE} "rsgain.c" "scan.cpp" "output.c" "tag.cpp" "easymode.cpp")
endif()
set (EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")



  

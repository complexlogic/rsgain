if (WIN32)
  add_executable(${EXECUTABLE_TITLE} "loudgain.c" "scan.c" "output.c" "tag.cc" "${PROJECT_BINARY_DIR}/loudgain.manifest")
else()
  add_executable(${EXECUTABLE_TITLE} "loudgain.c" "scan.c" "output.c" "tag.cc")
endif()
set (EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")



  

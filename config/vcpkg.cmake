message("Setting up vcpkg...")
include(FetchContent)
FetchContent_Declare(
  vcpkg
  GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
  GIT_SHALLOW TRUE
  SOURCE_DIR ${PROJECT_BINARY_DIR}
)
FetchContent_Declare(
  vcpkg_overlay
  GIT_REPOSITORY https://github.com/complexlogic/vcpkg.git
  GIT_TAG origin/rsgain
  GIT_SHALLOW TRUE
  SOURCE_DIR ${PROJECT_BINARY_DIR}
)
FetchContent_MakeAvailable(vcpkg vcpkg_overlay)
set(VCPKG_OVERLAY_PORTS "${CMAKE_BINARY_DIR}/_deps/vcpkg_overlay-src/ports")

if (WIN32)
  if (NOT VCPKG_MANIFEST_FEATURES)
    set(VCPKG_MANIFEST_FEATURES ffmpeg libebur128 inih)
  endif ()
  if (NOT VCPKG_OVERLAY_TRIPLETS)
    list(APPEND VCPKG_OVERLAY_TRIPLETS "${CMAKE_SOURCE_DIR}/config/vcpkg_triplets")
  endif ()
  if (NOT VCPKG_TARGET_TRIPLET)
    set (VCPKG_TARGET_TRIPLET "custom-triplet")
  endif ()
endif ()

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_BINARY_DIR}/_deps/vcpkg-src/scripts/buildsystems/vcpkg.cmake")

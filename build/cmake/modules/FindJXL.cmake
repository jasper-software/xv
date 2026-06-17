# FindJXL.cmake
# Try to find the JPEG XL library.

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_JXL QUIET libjxl libjxl_threads libjxl_cms)
endif()

find_path(JXL_INCLUDE_DIR
  NAMES jxl/decode.h jxl/encode.h
  HINTS ${PC_JXL_INCLUDEDIR} ${PC_JXL_INCLUDE_DIRS}
)

set(JXL_VERSION "")
if(JXL_INCLUDE_DIR AND EXISTS "${JXL_INCLUDE_DIR}/jxl/version.h")
  file(READ "${JXL_INCLUDE_DIR}/jxl/version.h" _jpegxl_version_h)
  string(REGEX MATCH "#define[ \t]+JPEGXL_MAJOR_VERSION[ \t]+([0-9]+)" _m "${_jpegxl_version_h}")
  set(_jpegxl_major "${CMAKE_MATCH_1}")
  string(REGEX MATCH "#define[ \t]+JPEGXL_MINOR_VERSION[ \t]+([0-9]+)" _m "${_jpegxl_version_h}")
  set(_jpegxl_minor "${CMAKE_MATCH_1}")
  string(REGEX MATCH "#define[ \t]+JPEGXL_PATCH_VERSION[ \t]+([0-9]+)" _m "${_jpegxl_version_h}")
  set(_jpegxl_patch "${CMAKE_MATCH_1}")
  if(NOT _jpegxl_major STREQUAL "" AND NOT _jpegxl_minor STREQUAL "" AND NOT _jpegxl_patch STREQUAL "")
    set(JXL_VERSION "${_jpegxl_major}.${_jpegxl_minor}.${_jpegxl_patch}")
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JXL
  REQUIRED_VARS JXL_INCLUDE_DIR
  VERSION_VAR JXL_VERSION
)

if(JXL_FOUND)
  set(JXL_INCLUDE_DIRS ${JXL_INCLUDE_DIR})

  string(JOIN " " JXL_LDFLAGS ${PC_JXL_LDFLAGS})
  string(JOIN " " JXL_CFLAGS ${PC_JXL_CFLAGS})

  if(NOT TARGET JXL::JXL)
    add_library(JXL::JXL UNKNOWN IMPORTED)
    set_target_properties(JXL::JXL PROPERTIES
      IMPORTED_LOCATION "${JXL_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${JXL_INCLUDE_DIR}"
    )
  endif()
endif()

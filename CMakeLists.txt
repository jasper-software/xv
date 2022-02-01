cmake_minimum_required(VERSION 3.12)

################################################################################
# Specify software version information.
################################################################################

# The major, minor, and micro version numbers of the project.
set(XV_VERSION_MAJOR 4)
set(XV_VERSION_MINOR 0)
set(XV_VERSION_PATCH 0)

# Generate the project version.
set(XV_VERSION "${XV_VERSION_MAJOR}.${XV_VERSION_MINOR}.${XV_VERSION_PATCH}")

project(xv VERSION ${XV_VERSION} LANGUAGES C)

################################################################################
# Options
################################################################################

option(XV_ENABLE_JPEG "Enable JPEG Support" ON)
option(XV_ENABLE_JP2K "Enable JP2K Support" ON)
option(XV_ENABLE_PDS  "Enable PDF Support" ON)
option(XV_ENABLE_PNG  "Enable PNG Support" ON)
option(XV_ENABLE_TIFF "Enable TIFF Support" ON)

################################################################################
# Include modules and set policies.
################################################################################

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/build/cmake/modules)

# Adhere to GNU filesystem layout conventions.
include(GNUInstallDirs)

include(CheckCCompilerFlag)
include(Sanitizers)

################################################################################
# Find libraries.
################################################################################

find_package(X11 REQUIRED)

find_library(MATH_LIBRARY m)
if(NOT MATH_LIBRARY)
	set(MATH_LIBRARY "")
endif()

find_package(Jasper)
if(XV_ENABLE_JP2K AND NOT TARGET Jasper::Jasper)
	message(WARNING "Disabling JPEG-2000 Support.")
	set(XV_ENABLE_JP2K OFF)
endif()

find_package(JPEG)
if(XV_ENABLE_JPEG AND NOT TARGET JPEG::JPEG)
	message(WARNING "Disabling JPEG-2000 Support.")
	set(XV_ENABLE_JPEG OFF)
endif()

find_package(PNG)
if(XV_ENABLE_PNG AND NOT TARGET PNG::PNG)
	message(WARNING "Disabling PNG Support.")
	set(XV_ENABLE_PNG OFF)
endif()

find_package(TIFF)
if(XV_ENABLE_TIFF AND NOT TARGET TIFF::TIFF)
	message(WARNING "Disabling TIFF Support.")
	set(XV_ENABLE_TIFF OFF)
endif()

message("JP2K: ${XV_ENABLE_JP2K}")
message("JPEG: ${XV_ENABLE_JPEG}")
message("TIFF: ${XV_ENABLE_TIFF}")
message("PNG: ${XV_ENABLE_PNG}")

################################################################################
# Subdirectories.
################################################################################

add_subdirectory(src)

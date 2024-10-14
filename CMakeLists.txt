cmake_minimum_required(VERSION 3.12)

################################################################################
# Specify software version information.
################################################################################

# The major, minor, and micro version numbers of the project.
set(XV_VERSION_MAJOR 6)
set(XV_VERSION_MINOR 0)
set(XV_VERSION_PATCH 2)
set(XV_VERSION_DATE "20241009")

# Generate the project version.
set(XV_VERSION "${XV_VERSION_MAJOR}.${XV_VERSION_MINOR}.${XV_VERSION_PATCH}")

set(XV_VERSTR "${XV_VERSION}-${XV_VERSION_DATE}")
set(XV_REVDATE "version ${XV_VERSION}-${XV_VERSION_DATE}")

project(xv VERSION ${XV_VERSION} LANGUAGES C)

################################################################################
# Options
################################################################################

option(XV_ENABLE_JPEG "Enable JPEG Support" ON)
option(XV_ENABLE_JP2K "Enable JP2K Support" ON)
option(XV_ENABLE_PDS  "Enable PDF Support" ON)
option(XV_ENABLE_PNG  "Enable PNG Support" ON)
option(XV_ENABLE_TIFF "Enable TIFF Support" ON)
option(XV_ENABLE_WEBP "Enable WEBP Support" ON)
option(XV_ENABLE_G3   "Enable G3 Support" ON)
option(XV_ENABLE_XRANDR "Enable XRANDR Support" ON)

option(XV_STRICT "Treat compiler warnings as errors" OFF)

################################################################################
# Include modules and set policies.
################################################################################

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/build/cmake/modules)

# Adhere to GNU filesystem layout conventions.
include(GNUInstallDirs)

include(CheckIncludeFiles)
include(CheckCCompilerFlag)
include(Sanitizers)

################################################################################
# Compiler settings.
################################################################################

# Prefer C99 (if available) but do not require it.
set(CMAKE_C_STANDARD 99)

set(CMAKE_C_EXTENSIONS ON)

if(APPLE AND CMAKE_SYSTEM_NAME MATCHES "Darwin")
	set(MACOS TRUE)
else()
	set(MACOS FALSE)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
	set(LINUX TRUE)
else()
	set(LINUX FALSE)
endif()

message("CMake version: ${CMAKE_VERSION}")

add_compile_definitions(XV_CMAKE_BUILD)

# Customize the compiler flags for warning messages.
if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
	#check_c_compiler_flag("-Wno-stringop-truncation"
	#  XV_GCC_WARN_NO_STRINGOP_TRUNCATION)
	add_compile_options(-Wall)
	add_compile_options(-pedantic)
	add_compile_options(-W)
	add_compile_options(-Wmissing-prototypes)
	add_compile_options(-Wstrict-prototypes)
	add_compile_options(-Wmissing-declarations)
	add_compile_options(-Wno-sign-compare)
	add_compile_options(-Wno-format-truncation)
	#if(XV_GCC_WARN_NO_STRINGOP_TRUNCATION)
	#	add_compile_options(-Wno-stringop-truncation)
	#endif()
	if(XV_STRICT)
		# Treat all compiler warnings as errors.
		add_compile_options(-pedantic-errors)
		add_compile_options(-Werror)
	endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
	# signed/unsigned mismatch
	add_compile_options(/wd4018)
	if(XV_STRICT)
		# Treat all compiler warnings as errors.
		add_compile_options(/WX)
	endif()
endif()

################################################################################
#
################################################################################

check_include_files(limits.h XV_HAVE_LIMITS_H)

check_c_source_compiles("
#include <string.h>
int main() {
	const char *p = strerror(0);
}
" XV_TRY_COMPILE_STRERROR)
set(XV_HAVE_STRERROR ${XV_TRY_COMPILE_STRERROR})

check_c_source_compiles("
#include <stdio.h> /* MacOS */
#include <errno.h>
int main() {
	const char *p = sys_errlist[0];
}
" XV_TRY_COMPILE_SYSERRLIST)
set(XV_HAVE_SYSERRLISTDECL ${XV_TRY_COMPILE_SYSERRLIST})

check_c_source_compiles("
#include <limits.h>
int main() {
	char x[NAME_MAX + 1];
}
" XV_TRY_COMPILE_NAMEMAX)
set(XV_HAVE_NAMEMAX ${XV_TRY_COMPILE_NAMEMAX})

################################################################################
# Find libraries.
################################################################################

find_package(X11 REQUIRED)
if (NOT TARGET X11::X11 OR NOT TARGET X11::Xt)
	message(FATAL_ERROR "X11/Xt library is missing")
endif()

find_library(MATH_LIBRARY m)
if(NOT MATH_LIBRARY)
	set(MATH_LIBRARY "")
endif()

find_package(Jasper)
if(JASPER_FOUND OR TARGET Jasper::Jasper)
	message("JASPER_INCLUDE_DIR: ${JASPER_INCLUDE_DIR}")
	message("JASPER_INCLUDE_DIRS: ${JASPER_INCLUDE_DIRS}")
	message("JASPER_LIBRARY: ${JASPER_LIBRARY}")
	message("JASPER_LIBRARIES: ${JASPER_LIBRARIES}")
else()
	message("JasPer library not found")
endif()
# CMake 3.22 or newer is required for the Jasper::Jasper imported target to
# be defined.  So, if the JasPer library is found but no imported target has
# been created, create one.
if(NOT TARGET Jasper::Jasper)
	if(JASPER_FOUND)
		add_library(Jasper::Jasper UNKNOWN IMPORTED)
		if(JASPER_INCLUDE_DIR)
			set_target_properties(Jasper::Jasper PROPERTIES
			  INTERFACE_INCLUDE_DIRECTORIES "${JASPER_INCLUDE_DIR}")
		endif()
		if(EXISTS "${JASPER_LIBRARY}")
			set_target_properties(Jasper::Jasper PROPERTIES
			  IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			  IMPORTED_LOCATION "${JASPER_LIBRARY}")
		endif()
	endif()
endif()
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

find_package(WebP)
if(XV_ENABLE_WEBP AND NOT TARGET WebP::libwebp)
    message(WARNING "Disabling WEBP Support.")
    set(XV_ENABLE_WEBP OFF)
endif()

if(XV_ENABLE_XRANDR)
	find_path(XRANDR_INCLUDE_DIR NAMES X11/extensions/Xrandr.h
		PATH_SUFFIXES X11/extensions
		DOC "The XRANDR include directory"
	)

	find_library(XRANDR_LIBRARY NAMES Xrandr
		DOC "The XRANDR library"
	)

	include(FindPackageHandleStandardArgs)
	FIND_PACKAGE_HANDLE_STANDARD_ARGS(XRANDR DEFAULT_MSG XRANDR_LIBRARY XRANDR_INCLUDE_DIR)

	mark_as_advanced(XRANDR_INCLUDE_DIR XRANDR_LIBRARY)

	if(XRANDR_FOUND)
		set(XRANDR_LIBRARIES ${XRANDR_LIBRARY})
		set(XRANDR_INCLUDE_DIRS ${XRANDR_INCLUDE_DIR})
	else()
		message(WARNING "Disabling RANDR Support.")
		set(XV_ENABLE_XRANDR OFF)
	endif()
endif()

message("JP2K: ${XV_ENABLE_JP2K}")
message("JPEG: ${XV_ENABLE_JPEG}")
message("TIFF: ${XV_ENABLE_TIFF}")
message("PNG: ${XV_ENABLE_PNG}")
message("WEBP: ${XV_ENABLE_WEBP}")
message("G3: ${XV_ENABLE_G3}")
message("RANDR: ${XV_ENABLE_XRANDR}")

################################################################################
# Subdirectories.
################################################################################

add_subdirectory(src)

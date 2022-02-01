option(XV_ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(XV_ENABLE_UBSAN "Enable Undefined-Behavior Sanitizer" OFF)
option(XV_ENABLE_LSAN "Enable Leak Sanitizer" OFF)
option(XV_ENABLE_MSAN "Enable Memory Sanitizer" OFF)
option(XV_ENABLE_TSAN "Enable Thread Sanitizer" OFF)

# FIXME - This does not work.
#check_c_compiler_flag("-fsanitize=address" XV_HAVE_FSANITIZE_ADDRESS)
set(XV_HAVE_FSANITIZE_ADDRESS 1)

check_c_compiler_flag("-fsanitize=leak" XV_HAVE_FSANITIZE_LEAK)

# FIXME - This does not work.
#check_c_compiler_flag("-fsanitize=memory" XV_HAVE_FSANITIZE_MEMORY)
set(XV_HAVE_FSANITIZE_MEMORY 1)

# FIXME - This does not work.
#check_c_compiler_flag("-fsanitize=thread" XV_HAVE_FSANITIZE_THREAD)
set(XV_HAVE_FSANITIZE_THREAD 1)

check_c_compiler_flag("-fsanitize=undefined" XV_HAVE_FSANITIZE_UNDEFINED)

check_c_compiler_flag("/fsanitize=address" XV_HAVE_MSVC_FSANITIZE_ADDRESS)

if(XV_ENABLE_MSAN)
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
	  CMAKE_C_COMPILER_ID STREQUAL GNU)
		if(XV_HAVE_FSANITIZE_MEMORY)
			message("Enabling Memory Sanitizer")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory")
			set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -fsanitize=memory")
		endif()
	endif()
endif()

if(XV_ENABLE_ASAN)
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
	  CMAKE_C_COMPILER_ID STREQUAL GNU)
		if(XV_HAVE_FSANITIZE_ADDRESS)
			message("Enabling Address Sanitizer")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
			set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -fsanitize=address")
		endif()
	elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
		if(XV_HAVE_MSVC_FSANITIZE_ADDRESS)
			message("Enabling Address Sanitizer")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address")
			set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} /fsanitize=address")
		endif()
	endif()
endif()

if(XV_ENABLE_LSAN)
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
	  CMAKE_C_COMPILER_ID STREQUAL GNU)
		if(XV_HAVE_FSANITIZE_LEAK)
			message("Enabling Leak Sanitizer")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=leak")
			set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -fsanitize=leak")
		endif()
	endif()
endif()

if(XV_ENABLE_UBSAN)
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
	  CMAKE_C_COMPILER_ID STREQUAL GNU)
		if(XV_HAVE_FSANITIZE_UNDEFINED)
			message("Enabling Undefined-Behavior Sanitizer")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
			set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -fsanitize=undefined")
		endif()
	endif()
endif()

if(XV_ENABLE_TSAN)
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
	  CMAKE_C_COMPILER_ID STREQUAL GNU)
		if(XV_HAVE_FSANITIZE_THREAD)
			message("Enabling Thread Sanitizer")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
			set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -fsanitize=thread")
		endif()
	endif()
endif()

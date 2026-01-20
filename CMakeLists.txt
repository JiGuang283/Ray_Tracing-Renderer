############################################################
# Computer Graphics Assignment Configuration
# Tested on Windows 10 and Ubuntu 16.04.1
# By Wencong Yang, yangwc3@mail2.sysu.edu.cn
############################################################
cmake_minimum_required(VERSION 3.5)

# You'd better build it in release mode for performance
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)

project(CGAssignment4)

# C++ 14 is required
set(CMAKE_CXX_STANDARD 14)

# SIMD / 架构优化开关
option(ENABLE_AVX2 "Enable AVX2 optimizations" ON)
option(ENABLE_MARCH_NATIVE "Enable -march=native (host-dependent)" OFF)

# 基础优化
if (MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /fp:fast")
else()
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ffast-math")
endif()

# 定义选项供用户控制
option(ENABLE_AVX2 "Enable AVX2/FMA instructions" ON)
option(ENABLE_MARCH_NATIVE "Enable -march=native optimization" ON)

if(MSVC)
	# MSVC 编译器
	if(ENABLE_AVX2)
		# MSVC 的 /arch:AVX2 自动包含 FMA3
		add_compile_options(/arch:AVX2)
		add_definitions(-D__AVX2__ -D__FMA__) # MSVC有时不自动定义这些宏，手动加上保险
		message(STATUS "AVX2/FMA enabled (MSVC)")
	endif()

	if(ENABLE_MARCH_NATIVE)
		message(WARNING "-march=native is not supported by MSVC. Ignoring.")
	endif()

else()
	# GCC / Clang 编译器
	include(CheckCXXCompilerFlag)

	if(ENABLE_MARCH_NATIVE)
		# 如果开启了 native，它会自动检测并开启所有支持的指令集
		check_cxx_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
		if(COMPILER_SUPPORTS_MARCH_NATIVE)
			add_compile_options(-march=native)
			message(STATUS "Architecture optimization: -march=native enabled")
		else()
			message(WARNING "Compiler does not support -march=native")
		endif()

	elseif(ENABLE_AVX2)
		# 如果没开 native 但开了 AVX2，手动添加 -mavx2 和 -mfma
		check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)

		if(COMPILER_SUPPORTS_AVX2)
			add_compile_options(-mavx2)
			message(STATUS "AVX2 enabled")

			# FMA 通常与 AVX2 一起使用，但也需要单独检查
			check_cxx_compiler_flag("-mfma" COMPILER_SUPPORTS_FMA)
			if(COMPILER_SUPPORTS_FMA)
				add_compile_options(-mfma)
				message(STATUS "FMA enabled")
			else()
				message(STATUS "FMA not supported by compiler, code might run slower")
			endif()
		else()
			message(WARNING "AVX2 requested but not supported by compiler")
		endif()
	endif()
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/core)
include_directories(${PROJECT_SOURCE_DIR}/src/geometry)
include_directories(${PROJECT_SOURCE_DIR}/src/materials)
include_directories(${PROJECT_SOURCE_DIR}/src/lighting)
include_directories(${PROJECT_SOURCE_DIR}/src/renderer)
include_directories(${PROJECT_SOURCE_DIR}/src/scene)
include_directories(${PROJECT_SOURCE_DIR}/src/external)
include_directories(${PROJECT_SOURCE_DIR}/src/external/imgui)
include_directories(${PROJECT_SOURCE_DIR}/src/external/imgui/backends)
include_directories(${PROJECT_SOURCE_DIR}/src/app)
include_directories(${PROJECT_SOURCE_DIR}/src/utils)

############################################################
# Windows or Linux options
############################################################
IF (CMAKE_SYSTEM_NAME MATCHES "Windows")
	link_directories(${PROJECT_SOURCE_DIR}/libs)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Linux")
	find_package(SDL2 REQUIRED)

	# check if boost was found
	if(SDL2_FOUND)
	    message ("SDL2 found")
	else()
	    message (FATAL_ERROR "Cannot find SDL2")
	endif()
ENDIF()

############################################################

############################################################
# If SDL2 was not found, please install SDL2 according to
# the following terminal commands:
# sudo apt-get update
# sudo apt-get install libsdl2-2.0
# sudo apt-get install libsdl2-dev
# sdl2-config --version
###########################################################


############################################################
# Create an executable
############################################################

# Recursively find all header and source files in src and its subdirectories
file(GLOB_RECURSE HEADERS ${PROJECT_SOURCE_DIR}/src/*.h)
file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

file(GLOB IMGUI_SOURCES
		src/external/imgui/*.cpp
		src/external/imgui/backends/*.cpp
)

source_group("Header Files" FILES ${HEADERS})

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${IMGUI_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})

# link the target with the SDL2
target_link_libraries( ${PROJECT_NAME} 
    PRIVATE 
        SDL2
        SDL2main
)

# link the target with the pthread (only for ubuntu)
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
	target_link_libraries( ${PROJECT_NAME} 
	    PRIVATE 
		pthread
	)
ENDIF()

# Force re-configure
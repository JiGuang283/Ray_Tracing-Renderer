############################################################
# Computer Graphics Assignment Configuration
# Tested on Windows 10 and Ubuntu 16.04.1
# By Wencong Yang, yangwc3@mail2.sysu.edu.cn
############################################################
cmake_minimum_required(VERSION 3.5)

# You'd better build it in release mode for performance
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)

project(CGAssignment4)

# C++ 11 is required
set(CMAKE_CXX_STANDARD 14)

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
cmake_minimum_required(VERSION 3.20)
project(VideoTranscoder VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Find required packages
find_package(Threads REQUIRED)

# Try multiple methods to find FFmpeg
set(FFMPEG_FOUND FALSE)

# Method 1: Try pkg-config first
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG QUIET 
        libavformat 
        libavcodec 
        libavutil
        libswscale
        libswresample
    )
    if(FFMPEG_FOUND)
        message(STATUS "Found FFmpeg via pkg-config")
    endif()
endif()

# Method 2: Try manual search for common locations
if(NOT FFMPEG_FOUND)
    # Common FFmpeg installation paths
    set(FFMPEG_SEARCH_PATHS
        /usr/local
        /usr
        /opt/homebrew  # macOS Homebrew ARM
        /opt/local     # MacPorts
        /mingw64       # MSYS2
        ${CMAKE_PREFIX_PATH}
    )
    
    # Find include directory
    find_path(FFMPEG_INCLUDE_DIR
        NAMES libavformat/avformat.h
        PATHS ${FFMPEG_SEARCH_PATHS}
        PATH_SUFFIXES include
    )
    
    # Find libraries
    find_library(AVFORMAT_LIBRARY
        NAMES avformat
        PATHS ${FFMPEG_SEARCH_PATHS}
        PATH_SUFFIXES lib lib64
    )
    
    find_library(AVCODEC_LIBRARY
        NAMES avcodec
        PATHS ${FFMPEG_SEARCH_PATHS}
        PATH_SUFFIXES lib lib64
    )
    
    find_library(AVUTIL_LIBRARY
        NAMES avutil
        PATHS ${FFMPEG_SEARCH_PATHS}
        PATH_SUFFIXES lib lib64
    )
    
    find_library(SWSCALE_LIBRARY
        NAMES swscale
        PATHS ${FFMPEG_SEARCH_PATHS}
        PATH_SUFFIXES lib lib64
    )
    
    find_library(SWRESAMPLE_LIBRARY
        NAMES swresample
        PATHS ${FFMPEG_SEARCH_PATHS}
        PATH_SUFFIXES lib lib64
    )
    
    if(FFMPEG_INCLUDE_DIR AND AVFORMAT_LIBRARY AND AVCODEC_LIBRARY AND AVUTIL_LIBRARY)
        set(FFMPEG_FOUND TRUE)
        set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR})
        set(FFMPEG_LIBRARIES 
            ${AVFORMAT_LIBRARY}
            ${AVCODEC_LIBRARY} 
            ${AVUTIL_LIBRARY}
        )
        
        if(SWSCALE_LIBRARY)
            list(APPEND FFMPEG_LIBRARIES ${SWSCALE_LIBRARY})
        endif()
        
        if(SWRESAMPLE_LIBRARY)
            list(APPEND FFMPEG_LIBRARIES ${SWRESAMPLE_LIBRARY})
        endif()
        
        message(STATUS "Found FFmpeg via manual search")
        message(STATUS "FFmpeg include dir: ${FFMPEG_INCLUDE_DIRS}")
        message(STATUS "FFmpeg libraries: ${FFMPEG_LIBRARIES}")
    endif()
endif()

# Method 3: System command fallback (no linking, just external calls)
if(NOT FFMPEG_FOUND)
    find_program(FFMPEG_EXECUTABLE ffmpeg)
    find_program(FFPROBE_EXECUTABLE ffprobe)
    
    if(FFMPEG_EXECUTABLE AND FFPROBE_EXECUTABLE)
        message(STATUS "FFmpeg development libraries not found, but ffmpeg/ffprobe executables detected")
        message(STATUS "Building in system-command mode (requires ffmpeg/ffprobe in PATH)")
        add_definitions(-DUSE_SYSTEM_FFMPEG)
        set(FFMPEG_SYSTEM_MODE TRUE)
    else()
        message(FATAL_ERROR "FFmpeg not found! Please install:\n"
                           "Fedora/RHEL: sudo dnf install ffmpeg-devel\n"
                           "Ubuntu/Debian: sudo apt install libavformat-dev libavcodec-dev libavutil-dev\n"
                           "macOS: brew install ffmpeg\n"
                           "Or ensure ffmpeg/ffprobe are in PATH")
    endif()
endif()

# Source files - UPDATE THESE TO MATCH YOUR ACTUAL FILENAMES
set(SOURCES
    src/main.cpp
    src/transcoder.cpp
)

# Headers - UPDATE THESE TO MATCH YOUR ACTUAL FILENAMES
set(HEADERS
    include/transcoder.h
)

# Debug: Print what files we're trying to include
message(STATUS "Source files:")
foreach(SRC ${SOURCES})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SRC})
        message(STATUS "  ✓ ${SRC}")
    else()
        message(STATUS "  ✗ ${SRC} (NOT FOUND)")
    endif()
endforeach()

# Create executable with ALL source files
add_executable(video_transcoder ${SOURCES} ${HEADERS})

# Include directory
target_include_directories(video_transcoder PRIVATE include)

# Link libraries
target_link_libraries(video_transcoder Threads::Threads)

if(FFMPEG_FOUND AND NOT FFMPEG_SYSTEM_MODE)
    target_include_directories(video_transcoder PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(video_transcoder ${FFMPEG_LIBRARIES})
    if(FFMPEG_CFLAGS_OTHER)
        target_compile_options(video_transcoder PRIVATE ${FFMPEG_CFLAGS_OTHER})
    endif()
endif()

# Installation
install(TARGETS video_transcoder 
    RUNTIME DESTINATION bin
)

# Print build info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
if(FFMPEG_FOUND AND NOT FFMPEG_SYSTEM_MODE)
    message(STATUS "FFmpeg mode: Library linking")
    message(STATUS "FFmpeg libraries: ${FFMPEG_LIBRARIES}")
elseif(FFMPEG_SYSTEM_MODE)
    message(STATUS "FFmpeg mode: System commands")
    message(STATUS "FFmpeg executable: ${FFMPEG_EXECUTABLE}")
    message(STATUS "FFprobe executable: ${FFPROBE_EXECUTABLE}")
endif()

cmake_minimum_required(VERSION 3.20)
project(AdvancedVideoTranscoder VERSION 2.0.0)

# C++20 required for advanced features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)

# FFmpeg is always required
pkg_check_modules(FFMPEG REQUIRED 
    libavformat 
    libavcodec 
    libavutil 
    libswscale 
    libswresample
)

# Optional FFTW for high-performance FFT
if(PkgConfig_FOUND)
    pkg_check_modules(FFTW3 fftw3f)
    if(FFTW3_FOUND)
        message(STATUS "FFTW3 found - enabling high-performance FFT")
    else()
        message(STATUS "FFTW3 not found - using fallback FFT implementation")
    endif()
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/transcoder.cpp
    src/audio_sync.cpp
)

# Header files for IDE support
set(HEADERS
    include/transcoder.h
    include/audio_sync.h
)

# Create executable
add_executable(video_transcoder ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(video_transcoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
)

# Conditional compilation flags
if(FFTW3_FOUND)
    target_compile_definitions(video_transcoder PRIVATE USE_FFTW)
    target_include_directories(video_transcoder PRIVATE ${FFTW3_INCLUDE_DIRS})
    target_link_libraries(video_transcoder ${FFTW3_LIBRARIES})
endif()

# Link libraries
target_link_libraries(video_transcoder 
    ${FFMPEG_LIBRARIES}
    m  # Math library
)

# Compiler flags
target_compile_options(video_transcoder PRIVATE 
    ${FFMPEG_CFLAGS_OTHER}
)

if(FFTW3_FOUND)
    target_compile_options(video_transcoder PRIVATE ${FFTW3_CFLAGS_OTHER})
endif()

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(video_transcoder PRIVATE
        $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native>
        $<$<CONFIG:Debug>:-O0 -g -Wall -Wextra>
    )
endif()

# Installation
install(TARGETS video_transcoder 
    RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Advanced Video Transcoder Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
if(FFTW3_FOUND)
    message(STATUS "FFTW3: Found (high-performance FFT enabled)")
else()
    message(STATUS "FFTW3: Not found (using fallback FFT)")
endif()
message(STATUS "Target: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "==============================================")
message(STATUS "")
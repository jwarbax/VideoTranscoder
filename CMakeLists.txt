cmake_minimum_required(VERSION 3.20)
project(VideoTranscoder)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswscale libswresample)

# Source files
set(SOURCES
    src/main.cpp
    src/transcoder.cpp
    src/audio_sync.cpp
)

# Create executable
add_executable(video_transcoder ${SOURCES})

# Include directories
target_include_directories(video_transcoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(video_transcoder 
    ${FFMPEG_LIBRARIES}
    m
)

# Compiler flags
target_compile_options(video_transcoder PRIVATE ${FFMPEG_CFLAGS_OTHER})
/**
 * @file main.cpp
 * @brief Minimal video transcoder with audio sync
 */

#include "transcoder.h"
#include <iostream>
#include <filesystem>

int main() {
    std::cout << "Video Transcoder with Audio Sync" << std::endl;
    std::cout << "================================" << std::endl;
    
    // Check input directory exists
    if (!std::filesystem::exists("/s3")) {
        std::cerr << "ERROR: /s3 directory not found" << std::endl;
        return 1;
    }
    
    // Create output directory
    std::filesystem::create_directories("/s3/output");
    
    // Run transcoder
    VideoTranscoder transcoder;
    bool success = transcoder.processAll("/s3", "/s3/output");
    
    if (success) {
        std::cout << "\n✅ Processing complete!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Processing failed!" << std::endl;
        return 1;
    }
}
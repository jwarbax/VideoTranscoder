/**
 * @file transcoder.cpp
 * @brief Minimal video transcoder implementation
 */

#include "transcoder.h"
#include "speech_sync.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>

VideoTranscoder::VideoTranscoder() {
    if (m_verbose) {
        std::cout << "VideoTranscoder initialized" << std::endl;
    }
}

bool VideoTranscoder::processAll(const std::filesystem::path& inputDir, 
                                const std::filesystem::path& outputDir) {
    
    std::cout << "\nProcessing files from: " << inputDir << std::endl;
    std::cout << "Output directory: " << outputDir << std::endl;
    
    // Find all video and audio files
    auto videoFiles = findVideoFiles(inputDir);
    auto audioFiles = findAudioFiles(inputDir);
    
    std::cout << "\nFound " << videoFiles.size() << " video files" << std::endl;
    std::cout << "Found " << audioFiles.size() << " audio files" << std::endl;
    
    if (videoFiles.empty()) {
        std::cout << "No video files found!" << std::endl;
        return false;
    }
    
    bool allSuccessful = true;
    
    // Process each video file
    for (const auto& videoFile : videoFiles) {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "Processing: " << videoFile.filename().string() << std::endl;
        
        // Find matching audio files
        auto [highGain, lowGain] = findAudioMatch(videoFile, audioFiles);
        
        if (highGain.empty()) {
            std::cout << "❌ No matching audio found for " << videoFile.filename().string() << std::endl;
            allSuccessful = false;
            continue;
        }
        
        std::cout << "✅ Matched with: " << highGain.filename().string() << std::endl;
        if (!lowGain.empty()) {
            std::cout << "✅ Low gain pair: " << lowGain.filename().string() << std::endl;
        }
        
        // Detect sync offset
        double offset = detectSyncOffset(videoFile, highGain);
        std::cout << "🎯 Detected offset: " << offset << "s" << std::endl;
        
        // Generate output filename
        std::string outputName = videoFile.stem().string() + ".mov";
        std::filesystem::path outputFile = outputDir / outputName;
        
        // Transcode
        bool success = transcodeVideo(videoFile, highGain, lowGain, offset, outputFile);
        
        if (success) {
            std::cout << "✅ Successfully transcoded to: " << outputName << std::endl;
        } else {
            std::cout << "❌ Failed to transcode: " << videoFile.filename().string() << std::endl;
            allSuccessful = false;
        }
    }
    
    return allSuccessful;
}

std::vector<std::filesystem::path> VideoTranscoder::findVideoFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> videoFiles;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                if (extension == ".mp4" || extension == ".MP4") {
                    videoFiles.push_back(entry.path());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error reading directory: " << e.what() << std::endl;
    }
    
    // Sort for consistent processing order
    std::sort(videoFiles.begin(), videoFiles.end());
    return videoFiles;
}

std::vector<std::filesystem::path> VideoTranscoder::findAudioFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> audioFiles;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                if (extension == ".wav" || extension == ".WAV") {
                    audioFiles.push_back(entry.path());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error reading directory: " << e.what() << std::endl;
    }
    
    std::sort(audioFiles.begin(), audioFiles.end());
    return audioFiles;
}

std::pair<std::filesystem::path, std::filesystem::path> VideoTranscoder::findAudioMatch(
    const std::filesystem::path& videoFile,
    const std::vector<std::filesystem::path>& audioFiles) {
    
    std::string videoStem = videoFile.stem().string();
    std::filesystem::path highGain, lowGain;
    
    // Look for exact filename match first
    for (const auto& audioFile : audioFiles) {
        std::string audioStem = audioFile.stem().string();
        
        if (audioStem == videoStem) {
            highGain = audioFile;
            if (m_verbose) {
                std::cout << "  Exact match: " << audioFile.filename().string() << std::endl;
            }
        }
        // Look for low gain version (with _D suffix)
        else if (audioStem == videoStem + "_D") {
            lowGain = audioFile;
            if (m_verbose) {
                std::cout << "  Low gain match: " << audioFile.filename().string() << std::endl;
            }
        }
    }
    
    // If no exact match, try duration-based matching
    if (highGain.empty()) {
        if (m_verbose) {
            std::cout << "  No exact match, trying duration-based matching..." << std::endl;
        }
        
        double videoDuration = getFileDuration(videoFile);
        double bestMatch = 999999.0;
        
        for (const auto& audioFile : audioFiles) {
            // Skip files that look like low gain versions
            std::string audioStem = audioFile.stem().string();
            if (audioStem.ends_with("_D")) continue;
            
            double audioDuration = getFileDuration(audioFile);
            double diff = std::abs(videoDuration - audioDuration);
            
            if (diff < 30.0 && diff < bestMatch) {  // Within 30 seconds
                bestMatch = diff;
                highGain = audioFile;
                
                // Look for corresponding low gain file
                std::string lowGainName = audioStem + "_D.wav";
                std::filesystem::path lowGainPath = audioFile.parent_path() / lowGainName;
                if (std::filesystem::exists(lowGainPath)) {
                    lowGain = lowGainPath;
                }
            }
        }
        
        if (!highGain.empty() && m_verbose) {
            std::cout << "  Duration match: " << highGain.filename().string() 
                      << " (diff: " << bestMatch << "s)" << std::endl;
        }
    }
    
    return {highGain, lowGain};
}

double VideoTranscoder::detectSyncOffset(const std::filesystem::path& videoFile,
                                        const std::filesystem::path& audioFile) {
    
    SpeechSync speechSync;
    speechSync.setVerbose(m_verbose);
    
    return speechSync.findOffset(videoFile, audioFile);
}

bool VideoTranscoder::transcodeVideo(const std::filesystem::path& videoFile,
                                   const std::filesystem::path& highGainAudio,
                                   const std::filesystem::path& lowGainAudio,
                                   double offset,
                                   const std::filesystem::path& outputFile) {
    
    std::ostringstream cmd;
    cmd << "ffmpeg -hide_banner -loglevel error -y ";
    
    // Input video
    cmd << "-i \"" << videoFile.string() << "\" ";
    
    // Input high gain audio with offset
    if (offset > 0.001) {
        cmd << "-itsoffset " << offset << " ";
    } else if (offset < -0.001) {
        cmd << "-ss " << (-offset) << " ";
    }
    cmd << "-i \"" << highGainAudio.string() << "\" ";
    
    // Input low gain audio with same offset (if available)
    if (!lowGainAudio.empty()) {
        if (offset > 0.001) {
            cmd << "-itsoffset " << offset << " ";
        } else if (offset < -0.001) {
            cmd << "-ss " << (-offset) << " ";
        }
        cmd << "-i \"" << lowGainAudio.string() << "\" ";
    }
    
    // Video encoding (low resolution for testing)
    cmd << "-c:v libx264 -preset fast -crf 23 -s 960x540 ";
    
    // Audio encoding (48kHz as requested)
    cmd << "-c:a pcm_s16le -ar 48000 ";
    
    // Audio mapping
    if (!lowGainAudio.empty()) {
        // 3 tracks: HighLav, LowLav, Camera
        cmd << "-map 0:v -map 1:a -map 2:a -map 0:a ";
        cmd << "-metadata:s:a:0 title=\"HighLav\" ";
        cmd << "-metadata:s:a:1 title=\"LowLav\" ";
        cmd << "-metadata:s:a:2 title=\"Camera\" ";
    } else {
        // 2 tracks: HighLav, Camera
        cmd << "-map 0:v -map 1:a -map 0:a ";
        cmd << "-metadata:s:a:0 title=\"HighLav\" ";
        cmd << "-metadata:s:a:1 title=\"Camera\" ";
    }
    
    // Output file
    cmd << "\"" << outputFile.string() << "\"";
    
    if (m_verbose) {
        std::cout << "FFmpeg command: " << cmd.str() << std::endl;
    }
    
    int result = std::system(cmd.str().c_str());
    return (result == 0);
}

double VideoTranscoder::getFileDuration(const std::filesystem::path& filepath) {
    std::string command = "ffprobe -v quiet -show_entries format=duration "
                         "-of csv=p=0 \"" + filepath.string() + "\" 2>/dev/null";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return 0.0;
    
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    
    try {
        return std::stod(result);
    } catch (...) {
        return 0.0;
    }
}
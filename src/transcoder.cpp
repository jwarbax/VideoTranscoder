/**
 * @file transcoder.cpp
 * @brief Advanced video transcoder implementation with hybrid audio sync
 */

#include "transcoder.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <chrono>

// ===========================
// SyncStatistics Implementation
// ===========================

void SyncStatistics::addResult(const SyncResult& result) {
    totalFiles++;
    
    if (result.confidence > 0.0f) {
        successfulSyncs++;
        avgConfidence = (avgConfidence * (successfulSyncs - 1) + result.confidence) / successfulSyncs;
    }
    
    if (result.confidence >= 0.8f) {
        highConfidenceSyncs++;
    }
    
    if (result.confidence < 0.3f && result.confidence > 0.0f) {
        fallbackSyncs++;
    }
    
    avgProcessingTime = (avgProcessingTime * (totalFiles - 1) + result.computationTime) / totalFiles;
    
    algorithmUsage[result.algorithm]++;
}

void SyncStatistics::printReport() const {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "📊 SYNCHRONIZATION STATISTICS REPORT" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    std::cout << "Total files processed: " << totalFiles << std::endl;
    std::cout << "Successful syncs: " << successfulSyncs << " (" 
              << std::fixed << std::setprecision(1) 
              << (totalFiles > 0 ? 100.0 * successfulSyncs / totalFiles : 0.0) << "%)" << std::endl;
    std::cout << "High confidence syncs: " << highConfidenceSyncs << " (" 
              << (totalFiles > 0 ? 100.0 * highConfidenceSyncs / totalFiles : 0.0) << "%)" << std::endl;
    std::cout << "Fallback syncs: " << fallbackSyncs << " (" 
              << (totalFiles > 0 ? 100.0 * fallbackSyncs / totalFiles : 0.0) << "%)" << std::endl;
    
    std::cout << "\nAverage confidence: " << std::setprecision(3) << avgConfidence << std::endl;
    std::cout << "Average processing time: " << std::setprecision(2) << avgProcessingTime << "s" << std::endl;
    
    if (!algorithmUsage.empty()) {
        std::cout << "\nAlgorithm usage:" << std::endl;
        for (const auto& pair : algorithmUsage) {
            std::cout << "  " << pair.first << ": " << pair.second << " times" << std::endl;
        }
    }
    
    std::cout << std::string(60, '=') << std::endl;
}

// ===========================
// VideoTranscoder Implementation
// ===========================

VideoTranscoder::VideoTranscoder() {
    audioSync = std::make_unique<HybridAudioSync>();
    
    if (verbose) {
        std::cout << "🎬 Advanced Video Transcoder Initialized" << std::endl;
        std::cout << "Features: Hybrid Audio Sync, Multi-Algorithm Processing" << std::endl;
    }
}

VideoTranscoder::~VideoTranscoder() = default;

bool VideoTranscoder::processAll(const std::filesystem::path& inputDir, 
                                const std::filesystem::path& outputDir,
                                SyncQuality syncQuality) {
    
    std::cout << "\n🚀 Starting Advanced Video Processing" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "Input directory: " << inputDir << std::endl;
    std::cout << "Output directory: " << outputDir << std::endl;
    std::cout << "Sync quality: ";
    switch (syncQuality) {
        case SyncQuality::REAL_TIME: std::cout << "Real-time (<20ms latency)"; break;
        case SyncQuality::STANDARD: std::cout << "Standard (balanced)"; break;
        case SyncQuality::HIGH_QUALITY: std::cout << "High Quality (maximum accuracy)"; break;
    }
    std::cout << std::endl;
    
    // Reset statistics
    statistics = SyncStatistics{};
    
    // Configure audio sync
    audioSync->setVerbose(verbose);
    audioSync->setQualityMode(syncQuality);
    
    // Find all video and audio files
    auto videoFiles = findVideoFiles(inputDir);
    auto audioFiles = findAudioFiles(inputDir);
    
    std::cout << "\n📁 File Discovery Results:" << std::endl;
    std::cout << "Found " << videoFiles.size() << " video files" << std::endl;
    std::cout << "Found " << audioFiles.size() << " audio files" << std::endl;
    
    if (videoFiles.empty()) {
        std::cout << "❌ No video files found!" << std::endl;
        return false;
    }
    
    bool allSuccessful = true;
    size_t processedCount = 0;
    
    // Process each video file
    for (const auto& videoFile : videoFiles) {
        processedCount++;
        
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "🎬 Processing (" << processedCount << "/" << videoFiles.size() 
                  << "): " << videoFile.filename().string() << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Find matching audio files
        auto [highGain, lowGain, matchConfidence] = findAudioMatch(videoFile, audioFiles);
        
        if (highGain.empty()) {
            std::cout << "⚠️  No matching audio found - ";
            if (fallbackProcessing) {
                std::cout << "proceeding with fallback processing" << std::endl;
                
                // Generate output filename
                std::string outputName = videoFile.stem().string() + ".mov";
                std::filesystem::path outputFile = outputDir / outputName;
                
                bool success = transcodeFallback(videoFile, outputFile);
                if (success) {
                    std::cout << "✅ Fallback transcoding successful: " << outputName << std::endl;
                } else {
                    std::cout << "❌ Fallback transcoding failed" << std::endl;
                    allSuccessful = false;
                }
                
                // Record empty sync result
                SyncResult emptyResult;
                statistics.addResult(emptyResult);
            } else {
                std::cout << "skipping file" << std::endl;
                allSuccessful = false;
            }
            continue;
        }
        
        std::cout << "🎵 Audio Match Results:" << std::endl;
        std::cout << "  High gain: " << highGain.filename().string() 
                  << " (confidence: " << matchConfidence << ")" << std::endl;
        if (!lowGain.empty()) {
            std::cout << "  Low gain: " << lowGain.filename().string() << std::endl;
        }
        
        // Perform advanced synchronization
        auto syncResult = detectAdvancedSync(videoFile, highGain, syncQuality);
        
        // Log detailed sync information
        logSyncDetails(videoFile, highGain, syncResult);
        
        // Validate sync result
        bool syncAcceptable = validateSyncResult(syncResult, videoFile, highGain);
        
        if (!syncAcceptable && !fallbackProcessing) {
            std::cout << "❌ Sync validation failed and fallback disabled - skipping" << std::endl;
            allSuccessful = false;
            statistics.addResult(syncResult);
            continue;
        }
        
        // Generate output filename
        std::string outputName = videoFile.stem().string() + ".mov";
        std::filesystem::path outputFile = outputDir / outputName;
        
        bool success = false;
        
        if (syncAcceptable) {
            // Proceed with synchronized transcoding
            success = transcodeWithSync(videoFile, highGain, lowGain, syncResult, outputFile);
            
            if (success) {
                std::cout << "✅ Synchronized transcoding successful: " << outputName << std::endl;
            } else {
                std::cout << "❌ Synchronized transcoding failed" << std::endl;
            }
        } else if (fallbackProcessing) {
            // Fallback to non-synchronized transcoding
            std::cout << "⚠️  Using fallback processing due to low sync confidence" << std::endl;
            success = transcodeFallback(videoFile, outputFile);
            
            if (success) {
                std::cout << "✅ Fallback transcoding successful: " << outputName << std::endl;
            } else {
                std::cout << "❌ Fallback transcoding failed" << std::endl;
            }
        }
        
        if (!success) {
            allSuccessful = false;
        }
        
        // Record statistics
        statistics.addResult(syncResult);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double>(endTime - startTime).count();
        std::cout << "⏱️  Total processing time: " << std::fixed << std::setprecision(2) 
                  << duration << "s" << std::endl;
    }
    
    // Print final statistics
    statistics.printReport();
    
    std::cout << "\n🏁 Processing Complete!" << std::endl;
    std::cout << "Overall success rate: " << std::fixed << std::setprecision(1)
              << (videoFiles.size() > 0 ? 100.0 * statistics.successfulSyncs / videoFiles.size() : 0.0)
              << "%" << std::endl;
    
    return allSuccessful;
}

std::vector<std::filesystem::path> VideoTranscoder::findVideoFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> videoFiles;
    
    const std::vector<std::string> videoExtensions = {".mp4", ".MP4", ".mov", ".MOV"};
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                
                if (std::find(videoExtensions.begin(), videoExtensions.end(), extension) != videoExtensions.end()) {
                    videoFiles.push_back(entry.path());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Error reading directory: " << e.what() << std::endl;
    }
    
    // Sort for consistent processing order
    std::sort(videoFiles.begin(), videoFiles.end());
    return videoFiles;
}

std::vector<std::filesystem::path> VideoTranscoder::findAudioFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> audioFiles;
    
    const std::vector<std::string> audioExtensions = {".wav", ".WAV"};
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                
                if (std::find(audioExtensions.begin(), audioExtensions.end(), extension) != audioExtensions.end()) {
                    audioFiles.push_back(entry.path());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Error reading directory: " << e.what() << std::endl;
    }
    
    std::sort(audioFiles.begin(), audioFiles.end());
    return audioFiles;
}

std::tuple<std::filesystem::path, std::filesystem::path, float> VideoTranscoder::findAudioMatch(
    const std::filesystem::path& videoFile,
    const std::vector<std::filesystem::path>& audioFiles) {
    
    std::string videoStem = videoFile.stem().string();
    std::filesystem::path highGain, lowGain;
    float matchConfidence = 0.0f;
    
    if (verbose) {
        std::cout << "🔍 Searching for audio matches for: " << videoStem << std::endl;
    }
    
    // Strategy 1: Exact filename match (highest confidence)
    for (const auto& audioFile : audioFiles) {
        std::string audioStem = audioFile.stem().string();
        
        if (audioStem == videoStem) {
            highGain = audioFile;
            matchConfidence = 1.0f;
            if (verbose) {
                std::cout << "  ✅ Exact filename match: " << audioFile.filename().string() << std::endl;
            }
        }
        // Look for low gain version (with _D suffix)
        else if (audioStem == videoStem + "_D") {
            lowGain = audioFile;
            if (verbose) {
                std::cout << "  ✅ Low gain pair found: " << audioFile.filename().string() << std::endl;
            }
        }
    }
    
    // Strategy 2: Duration-based matching (medium confidence)
    if (highGain.empty()) {
        if (verbose) {
            std::cout << "  🔍 No exact match found, trying duration-based matching..." << std::endl;
        }
        
        double videoDuration = getFileDuration(videoFile);
        double bestDurationDiff = std::numeric_limits<double>::max();
        std::filesystem::path bestMatch;
        
        for (const auto& audioFile : audioFiles) {
            // Skip files that look like low gain versions for primary matching
            std::string audioStem = audioFile.stem().string();
            if (audioStem.ends_with("_D")) continue;
            
            double audioDuration = getFileDuration(audioFile);
            
            if (isDurationCompatible(videoDuration, audioDuration)) {
                double diff = std::abs(videoDuration - audioDuration);
                
                if (diff < bestDurationDiff) {
                    bestDurationDiff = diff;
                    bestMatch = audioFile;
                }
            }
        }
        
        if (!bestMatch.empty()) {
            highGain = bestMatch;
            
            // Calculate confidence based on duration difference
            matchConfidence = std::max(0.3f, 1.0f - static_cast<float>(bestDurationDiff) / 30.0f);
            
            if (verbose) {
                std::cout << "  ✅ Duration-based match: " << bestMatch.filename().string() 
                          << " (diff: " << bestDurationDiff << "s, confidence: " << matchConfidence << ")" << std::endl;
            }
            
            // Look for corresponding low gain file
            std::string audioStem = bestMatch.stem().string();
            std::string lowGainName = audioStem + "_D.wav";
            std::filesystem::path lowGainPath = bestMatch.parent_path() / lowGainName;
            
            if (std::filesystem::exists(lowGainPath)) {
                lowGain = lowGainPath;
                if (verbose) {
                    std::cout << "  ✅ Found corresponding low gain: " << lowGainName << std::endl;
                }
            }
        }
    }
    
    // Strategy 3: Pattern matching with edit distance (low confidence)
    if (highGain.empty()) {
        if (verbose) {
            std::cout << "  🔍 Trying pattern-based matching..." << std::endl;
        }
        
        int bestEditDistance = std::numeric_limits<int>::max();
        std::filesystem::path bestPatternMatch;
        
        for (const auto& audioFile : audioFiles) {
            std::string audioStem = audioFile.stem().string();
            if (audioStem.ends_with("_D")) continue;
            
            // Simple edit distance calculation
            int editDistance = 0;
            size_t minLen = std::min(videoStem.length(), audioStem.length());
            
            for (size_t i = 0; i < minLen; ++i) {
                if (std::tolower(videoStem[i]) != std::tolower(audioStem[i])) {
                    editDistance++;
                }
            }
            editDistance += std::abs(static_cast<int>(videoStem.length()) - static_cast<int>(audioStem.length()));
            
            if (editDistance < bestEditDistance && editDistance <= 3) {
                bestEditDistance = editDistance;
                bestPatternMatch = audioFile;
            }
        }
        
        if (!bestPatternMatch.empty()) {
            highGain = bestPatternMatch;
            matchConfidence = std::max(0.1f, 1.0f - bestEditDistance / 10.0f);
            
            if (verbose) {
                std::cout << "  ✅ Pattern-based match: " << bestPatternMatch.filename().string() 
                          << " (edit distance: " << bestEditDistance << ", confidence: " << matchConfidence << ")" << std::endl;
            }
        }
    }
    
    return std::make_tuple(highGain, lowGain, matchConfidence);
}

SyncResult VideoTranscoder::detectAdvancedSync(const std::filesystem::path& videoFile,
                                              const std::filesystem::path& audioFile,
                                              SyncQuality quality) {
    
    if (verbose) {
        std::cout << "🎯 Starting advanced synchronization analysis..." << std::endl;
    }
    
    auto result = audioSync->findOptimalSync(videoFile, audioFile, quality);
    
    return result;
}

bool VideoTranscoder::validateSyncResult(const SyncResult& result,
                                        const std::filesystem::path& videoFile,
                                        const std::filesystem::path& audioFile) {
    
    if (verbose) {
        std::cout << "🔍 Validating sync result..." << std::endl;
    }
    
    // Check confidence threshold
    if (result.confidence < confidenceThreshold) {
        if (verbose) {
            std::cout << "  ❌ Confidence too low: " << result.confidence 
                      << " < " << confidenceThreshold << std::endl;
        }
        return false;
    }
    
    // Check offset reasonableness
    if (std::abs(result.offset) > 30.0) {
        if (verbose) {
            std::cout << "  ❌ Offset too large: " << result.offset << "s" << std::endl;
        }
        return false;
    }
    
    // Check duration compatibility
    double videoDuration = getFileDuration(videoFile);
    double audioDuration = getFileDuration(audioFile);
    
    if (!isDurationCompatible(videoDuration, audioDuration, 60.0)) {
        if (verbose) {
            std::cout << "  ❌ Duration mismatch: video=" << videoDuration 
                      << "s, audio=" << audioDuration << "s" << std::endl;
        }
        return false;
    }
    
    if (verbose) {
        std::cout << "  ✅ Sync result validation passed" << std::endl;
    }
    
    return true;
}

bool VideoTranscoder::transcodeWithSync(const std::filesystem::path& videoFile,
                                       const std::filesystem::path& highGainAudio,
                                       const std::filesystem::path& lowGainAudio,
                                       const SyncResult& syncResult,
                                       const std::filesystem::path& outputFile) {
    
    if (verbose) {
        std::cout << "🎬 Starting synchronized transcoding..." << std::endl;
        std::cout << "  Video: " << videoFile.filename().string() << std::endl;
        std::cout << "  High gain audio: " << highGainAudio.filename().string() << std::endl;
        if (!lowGainAudio.empty()) {
            std::cout << "  Low gain audio: " << lowGainAudio.filename().string() << std::endl;
        }
        std::cout << "  Sync offset: " << syncResult.offset << "s" << std::endl;
        std::cout << "  Algorithm used: " << syncResult.algorithm << std::endl;
    }
    
    std::ostringstream cmd;
    cmd << "ffmpeg -hide_banner -loglevel error -y ";
    
    // Input video
    cmd << "-i \"" << videoFile.string() << "\" ";
    
    // Input high gain audio with sync offset
    if (syncResult.offset > 0.001) {
        cmd << "-itsoffset " << std::fixed << std::setprecision(6) << syncResult.offset << " ";
    } else if (syncResult.offset < -0.001) {
        cmd << "-ss " << std::fixed << std::setprecision(6) << (-syncResult.offset) << " ";
    }
    cmd << "-i \"" << highGainAudio.string() << "\" ";
    
    // Input low gain audio with same offset (if available)
    if (!lowGainAudio.empty()) {
        if (syncResult.offset > 0.001) {
            cmd << "-itsoffset " << std::fixed << std::setprecision(6) << syncResult.offset << " ";
        } else if (syncResult.offset < -0.001) {
            cmd << "-ss " << std::fixed << std::setprecision(6) << (-syncResult.offset) << " ";
        }
        cmd << "-i \"" << lowGainAudio.string() << "\" ";
    }
    
    // Video encoding settings (professional quality)
    cmd << "-c:v prores_ks -profile:v 2 "; // ProRes 422 HQ
    cmd << "-vendor apl0 -bits_per_mb 8000 ";
    
    // Audio encoding settings (professional quality)
    cmd << "-c:a pcm_s24le -ar 48000 ";
    
    // Audio mapping and metadata
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
    
    // Add sync metadata
    cmd << "-metadata sync_algorithm=\"" << syncResult.algorithm << "\" ";
    cmd << "-metadata sync_offset=\"" << syncResult.offset << "\" ";
    cmd << "-metadata sync_confidence=\"" << syncResult.confidence << "\" ";
    
    // Output file
    cmd << "\"" << outputFile.string() << "\"";
    
    if (verbose) {
        std::cout << "  🔧 FFmpeg command: " << cmd.str() << std::endl;
    }
    
    int result = std::system(cmd.str().c_str());
    
    if (result == 0 && verbose) {
        std::cout << "  ✅ Transcoding completed successfully" << std::endl;
    } else if (result != 0 && verbose) {
        std::cout << "  ❌ Transcoding failed with exit code: " << result << std::endl;
    }
    
    return (result == 0);
}

bool VideoTranscoder::transcodeFallback(const std::filesystem::path& videoFile,
                                       const std::filesystem::path& outputFile) {
    
    if (verbose) {
        std::cout << "🔄 Starting fallback transcoding (video only)..." << std::endl;
    }
    
    std::ostringstream cmd;
    cmd << "ffmpeg -hide_banner -loglevel error -y ";
    cmd << "-i \"" << videoFile.string() << "\" ";
    
    // Video encoding (same as synchronized version)
    cmd << "-c:v prores_ks -profile:v 2 ";
    cmd << "-vendor apl0 -bits_per_mb 8000 ";
    
    // Audio encoding (camera audio only)
    cmd << "-c:a pcm_s24le -ar 48000 ";
    
    // Single track: Camera audio only
    cmd << "-map 0:v -map 0:a ";
    cmd << "-metadata:s:a:0 title=\"Camera\" ";
    cmd << "-metadata sync_method=\"fallback\" ";
    
    cmd << "\"" << outputFile.string() << "\"";
    
    if (verbose) {
        std::cout << "  🔧 FFmpeg command: " << cmd.str() << std::endl;
    }
    
    int result = std::system(cmd.str().c_str());
    
    if (result == 0 && verbose) {
        std::cout << "  ✅ Fallback transcoding completed successfully" << std::endl;
    } else if (result != 0 && verbose) {
        std::cout << "  ❌ Fallback transcoding failed with exit code: " << result << std::endl;
    }
    
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

bool VideoTranscoder::isDurationCompatible(double duration1, double duration2, double tolerance) {
    return std::abs(duration1 - duration2) <= tolerance;
}

void VideoTranscoder::logSyncDetails(const std::filesystem::path& videoFile,
                                    const std::filesystem::path& audioFile,
                                    const SyncResult& result) {
    std::cout << "\n📊 Synchronization Analysis Results:" << std::endl;
    std::cout << "  Algorithm: " << result.algorithm << std::endl;
    std::cout << "  Offset: " << std::fixed << std::setprecision(3) << result.offset << "s";
    
    if (result.offset > 0) {
        std::cout << " (audio starts after video)";
    } else if (result.offset < 0) {
        std::cout << " (audio starts before video)";
    } else {
        std::cout << " (perfect sync)";
    }
    std::cout << std::endl;
    
    std::cout << "  Confidence: " << std::setprecision(2) << result.confidence;
    if (result.confidence >= 0.8f) {
        std::cout << " (High) ✅";
    } else if (result.confidence >= 0.5f) {
        std::cout << " (Medium) ⚠️";
    } else if (result.confidence >= 0.3f) {
        std::cout << " (Low) 🔴";
    } else {
        std::cout << " (Very Low) ❌";
    }
    std::cout << std::endl;
    
    std::cout << "  Processing time: " << std::setprecision(3) << result.computationTime << "s" << std::endl;
    
    // Additional context
    double videoDuration = getFileDuration(videoFile);
    double audioDuration = getFileDuration(audioFile);
    std::cout << "  Duration compatibility: video=" << std::setprecision(1) << videoDuration 
              << "s, audio=" << audioDuration << "s (diff=" 
              << std::abs(videoDuration - audioDuration) << "s)" << std::endl;
}

void VideoTranscoder::setVerbose(bool verbose) {
    this->verbose = verbose;
    if (audioSync) {
        audioSync->setVerbose(verbose);
    }
}

const SyncStatistics& VideoTranscoder::getSyncStatistics() const {
    return statistics;
}

void VideoTranscoder::setConfidenceThreshold(float threshold) {
    confidenceThreshold = std::clamp(threshold, 0.0f, 1.0f);
    if (verbose) {
        std::cout << "🎯 Confidence threshold set to: " << confidenceThreshold << std::endl;
    }
}

void VideoTranscoder::setFallbackProcessing(bool enableFallback) {
    fallbackProcessing = enableFallback;
    if (verbose) {
        std::cout << "🔄 Fallback processing: " << (enableFallback ? "enabled" : "disabled") << std::endl;
    }
}
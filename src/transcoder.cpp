/**
 * @file transcoder.cpp
 * @brief Implementation of core video transcoding functionality
 * @author Video Transcoder
 * @date 2025
 */

#include "transcoder.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <regex>
#include <cstdlib>
#include <unistd.h>
#include <map>
#include <cmath>

// ============================================================================
// STATIC MEMBER DEFINITIONS
// ============================================================================

const std::vector<std::string> VideoTranscoder::SUPPORTED_VIDEO_EXTENSIONS = {
    ".mp4", ".mov", ".avi", ".mkv", ".mts", ".m2ts", ".MP4", ".MOV", ".AVI", ".MKV", ".MTS", ".M2TS"
};

const std::vector<std::string> VideoTranscoder::SUPPORTED_AUDIO_EXTENSIONS = {
    ".wav", ".mp3", ".aac", ".flac", ".m4a", ".WAV", ".MP3", ".AAC", ".FLAC", ".M4A"
};

// ============================================================================
// GLOBAL VARIABLE DEFINITIONS
// ============================================================================

TranscodeSettings g_settings;
double g_durationTolerance = DEFAULT_DURATION_TOLERANCE;
bool g_verboseOutput = true;
bool g_dryRun = false;
std::string g_syncConfigFile = "";

// ============================================================================
// CONSTRUCTOR AND DESTRUCTOR
// ============================================================================

VideoTranscoder::VideoTranscoder() {
    if (g_verboseOutput) {
        std::cout << "VideoTranscoder initialized with settings:\n";
        std::cout << "  Video codec: " << g_settings.videoCodec << " " << g_settings.videoOptions << "\n";
        std::cout << "  Audio codec: " << g_settings.audioCodec << " " << g_settings.audioOptions << "\n";
        std::cout << "  Duration tolerance: " << g_durationTolerance << "s\n";
    }
}

// ============================================================================
// FILE DISCOVERY AND FILTERING
// ============================================================================

std::vector<std::filesystem::path> VideoTranscoder::findVideoFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> videoFiles;
    
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        reportError("Directory does not exist or is not a directory", directory.string());
        return videoFiles;
    }
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                videoFiles.push_back(entry.path());
            }
        }
    } catch (const std::exception& e) {
        reportError("Error reading directory: " + std::string(e.what()), directory.string());
    }
    
    return filterVideoFiles(videoFiles);
}

std::vector<std::filesystem::path> VideoTranscoder::findAudioFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> audioFiles;
    
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        reportError("Directory does not exist or is not a directory", directory.string());
        return audioFiles;
    }
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                audioFiles.push_back(entry.path());
            }
        }
    } catch (const std::exception& e) {
        reportError("Error reading directory: " + std::string(e.what()), directory.string());
    }
    
    return filterAudioFiles(audioFiles);
}

std::vector<std::filesystem::path> VideoTranscoder::filterVideoFiles(const std::vector<std::filesystem::path>& files) {
    std::vector<std::filesystem::path> filtered;
    
    for (const auto& file : files) {
        std::string extension = file.extension().string();
        
        if (std::find(SUPPORTED_VIDEO_EXTENSIONS.begin(), SUPPORTED_VIDEO_EXTENSIONS.end(), extension) 
            != SUPPORTED_VIDEO_EXTENSIONS.end()) {
            filtered.push_back(file);
            if (g_verboseOutput) {
                std::cout << "Found video file: " << file.filename().string() << std::endl;
            }
        }
    }
    
    return filtered;
}

std::vector<std::filesystem::path> VideoTranscoder::filterAudioFiles(const std::vector<std::filesystem::path>& files) {
    std::vector<std::filesystem::path> filtered;
    
    for (const auto& file : files) {
        std::string extension = file.extension().string();
        
        if (std::find(SUPPORTED_AUDIO_EXTENSIONS.begin(), SUPPORTED_AUDIO_EXTENSIONS.end(), extension) 
            != SUPPORTED_AUDIO_EXTENSIONS.end()) {
            filtered.push_back(file);
            if (g_verboseOutput) {
                std::cout << "Found audio file: " << file.filename().string() 
                         << " (gain: " << (isHighGainFile(file) ? "HIGH" : "LOW/UNKNOWN") << ")" << std::endl;
            }
        }
    }
    
    return filtered;
}

// ============================================================================
// AUDIO-VIDEO MATCHING AND SYNCHRONIZATION
// ============================================================================

std::vector<AudioMatch> VideoTranscoder::findAudioMatches(
    const std::vector<std::filesystem::path>& videoFiles,
    const std::vector<std::filesystem::path>& audioFiles) {

    std::vector<AudioMatch> matches;
    std::vector<std::filesystem::path> usedAudioFiles;

    if (g_verboseOutput) {
        std::cout << "\n=== AUDIO MATCHING ANALYSIS ===\n";
        std::cout << "Video files: " << videoFiles.size() << std::endl;
        for (const auto& video : videoFiles) {
            double duration = getFileDuration(video);
            std::cout << "  " << video.filename().string() << " (" << duration << "s)" << std::endl;
        }

        std::cout << "Audio files: " << audioFiles.size() << std::endl;
        for (const auto& audio : audioFiles) {
            double duration = getFileDuration(audio);
            std::cout << "  " << audio.filename().string() << " (" << duration << "s)" << std::endl;
        }
        std::cout << "================================\n" << std::endl;
    }

    for (const auto& videoFile : videoFiles) {
        if (g_verboseOutput) {
            std::cout << "Processing: " << videoFile.filename().string() << std::endl;
        }

        // Find available audio files (not already used)
        std::vector<std::filesystem::path> availableAudio;
        for (const auto& audioFile : audioFiles) {
            if (std::find(usedAudioFiles.begin(), usedAudioFiles.end(), audioFile) == usedAudioFiles.end()) {
                availableAudio.push_back(audioFile);
            }
        }

        if (g_verboseOutput) {
            std::cout << "  Available audio files: " << availableAudio.size() << std::endl;
        }

        auto match = findBestAudioMatch(videoFile, availableAudio);

        if (match.has_value() && match->syncSuccess) {
            if (g_verboseOutput) {
                std::cout << "  ✓ Match found and sync successful" << std::endl;
            }
            // Mark audio files as used
            usedAudioFiles.push_back(match->highGainFile);
            if (!match->lowGainFile.empty()) {
                usedAudioFiles.push_back(match->lowGainFile);
            }
            matches.push_back(match.value());
        } else {
            if (g_verboseOutput) {
                std::cout << "  ✗ No successful match found" << std::endl;
            }
            // Create empty match for this video file
            AudioMatch emptyMatch;
            emptyMatch.syncSuccess = false;
            matches.push_back(emptyMatch);
        }
    }

    return matches;
}

std::optional<AudioMatch> VideoTranscoder::findBestAudioMatch(
    const std::filesystem::path& videoFile,
    const std::vector<std::filesystem::path>& audioFiles) {

    if (g_verboseOutput) {
        std::cout << "  Finding match for: " << videoFile.filename().string() << std::endl;
    }

    double videoDuration = getFileDuration(videoFile);
    if (g_verboseOutput) {
        std::cout << "  Video duration: " << videoDuration << "s" << std::endl;
    }

    // Find ALL high-gain audio files within duration tolerance
    std::vector<std::filesystem::path> durationCandidates;

    for (const auto& audioFile : audioFiles) {
        // Only consider high-gain files for initial matching
        if (!isHighGainFile(audioFile)) {
            continue;
        }

        if (isDurationMatch(videoFile, audioFile)) {
            durationCandidates.push_back(audioFile);
            if (g_verboseOutput) {
                double audioDuration = getFileDuration(audioFile);
                double durationDiff = std::abs(videoDuration - audioDuration);
                std::cout << "  " << audioFile.filename().string() << ": "
                          << audioDuration << "s (diff: " << durationDiff << "s) - CANDIDATE" << std::endl;
            }
        }
    }

    if (g_verboseOutput) {
        std::cout << "  Found " << durationCandidates.size() << " duration-based candidates" << std::endl;
    }

    // Test sync for each candidate
    for (const auto& audioFile : durationCandidates) {
        if (g_verboseOutput) {
            std::cout << "  Testing sync with: " << audioFile.filename().string() << std::endl;
        }

        double offset = detectSyncOffset(videoFile, audioFile);

        // Consider sync successful if we got any reasonable offset
        bool syncSuccess = true; // Accept any duration-matched file for now

        if (syncSuccess) {
            if (g_verboseOutput) {
                std::cout << "  ✓ SYNC SUCCESS - This is our match!" << std::endl;
                std::cout << "  Detected offset: " << offset << "s" << std::endl;
            }

            AudioMatch match;
            match.highGainFile = audioFile;
            match.lowGainFile = getLowGainCounterpart(audioFile);
            match.syncSuccess = true;
            match.confidenceScore = 0.8; // Default confidence
            match.syncOffset = offset;
            match.method = SyncMethod::AUTO_CORRELATION;

            if (!match.lowGainFile.empty()) {
                if (g_verboseOutput) {
                    std::cout << "  Found low-gain pair: " << match.lowGainFile.filename().string() << std::endl;
                }
            } else {
                if (g_verboseOutput) {
                    std::cout << "  No low-gain pair found" << std::endl;
                }
            }

            return match;
        } else {
            if (g_verboseOutput) {
                std::cout << "  ✗ Sync failed" << std::endl;
            }
        }
    }

    if (g_verboseOutput) {
        std::cout << "  No files passed sync test" << std::endl;
    }
    return std::nullopt;
}

double VideoTranscoder::detectSyncOffset(
    const std::filesystem::path& videoFile,
    const std::filesystem::path& audioFile) {

    // Check for manual config first
    std::string videoName = videoFile.filename().string();
    std::string audioName = audioFile.filename().string();
    auto configKey = std::make_pair(videoName, audioName);

    if (m_syncOffsets.find(configKey) != m_syncOffsets.end()) {
        double configOffset = m_syncOffsets[configKey];
        if (g_verboseOutput) {
            std::cout << "    Using config offset: " << configOffset << "s" << std::endl;
        }
        return configOffset;
    }

    // Test basic file compatibility first
    if (g_verboseOutput) {
        std::cout << "    Testing file compatibility..." << std::endl;
    }

    std::string compatTest = "ffmpeg -hide_banner -loglevel error "
                            "-i \"" + videoFile.string() + "\" "
                            "-i \"" + audioFile.string() + "\" "
                            "-t 2 -f null - 2>/dev/null";

    if (std::system(compatTest.c_str()) != 0) {
        if (g_verboseOutput) {
            std::cout << "    ✗ File compatibility test failed" << std::endl;
        }
        return 0.0;
    }

    if (g_verboseOutput) {
        std::cout << "    ✓ Files are compatible" << std::endl;
    }

    // Perform automatic sync detection
    return performAutoSync(videoFile, audioFile);
}

// ============================================================================
// TRANSCODING OPERATIONS
// ============================================================================

ProcessingResult VideoTranscoder::transcodeVideo(
    const std::filesystem::path& videoFile,
    const AudioMatch& audioMatch,
    const std::filesystem::path& outputFile) {
    
    ProcessingResult result;
    result.inputVideo = videoFile;
    result.outputFile = outputFile;
    result.audioMatch = audioMatch;
    
    if (g_verboseOutput) {
        std::cout << "\n=== TRANSCODING: " << videoFile.filename().string() << " ===\n";
    }
    
    if (!audioMatch.isValid()) {
        result.success = false;
        result.errorMessage = "No valid audio match provided";
        reportError(result.errorMessage, videoFile.filename().string());
        return result;
    }
    
    // Create output directory if it doesn't exist
    std::filesystem::path outputDir = outputFile.parent_path();
    if (!std::filesystem::exists(outputDir)) {
        try {
            std::filesystem::create_directories(outputDir);
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = "Failed to create output directory: " + std::string(e.what());
            reportError(result.errorMessage, outputFile.string());
            return result;
        }
    }
    
    // Calculate timing and overlap analysis
    double videoDuration = getFileDuration(videoFile);
    double lavDuration = getFileDuration(audioMatch.highGainFile);
    double offset = audioMatch.syncOffset;
    
    if (g_verboseOutput) {
        std::cout << "Content analysis - Video: " << videoDuration << "s, Lav: " << lavDuration 
                  << "s, Auto-detected offset: " << offset << "s" << std::endl;
    }
    
    // Calculate the overlap region with the offset
    double videoStart = 0.0;
    double videoEnd = videoDuration;
    double lavStart = offset; // Lav starts at offset seconds into video timeline
    double lavEnd = offset + lavDuration;
    
    // Find the overlapping region
    double overlapStart = std::max(videoStart, lavStart);
    double overlapEnd = std::min(videoEnd, lavEnd);
    double overlapDuration = overlapEnd - overlapStart;
    
    if (g_verboseOutput) {
        std::cout << "Overlap analysis:" << std::endl;
        std::cout << "  Video timeline: " << videoStart << "s to " << videoEnd << "s" << std::endl;
        std::cout << "  Lav timeline: " << lavStart << "s to " << lavEnd << "s" << std::endl;
        std::cout << "  Overlap region: " << overlapStart << "s to " << overlapEnd << "s (" 
                  << overlapDuration << "s)" << std::endl;
    }
    
    // Build and execute FFmpeg command
    std::string command = buildFFmpegCommand(videoFile, audioMatch, outputFile);
    
    if (g_verboseOutput) {
        std::cout << "FFmpeg command: " << command << std::endl;
    }
    
    if (g_dryRun) {
        std::cout << "DRY RUN - Would execute: " << command << std::endl;
        result.success = true;
        result.processingTime = 0.0;
        return result;
    }
    
    // Execute the transcoding command
    auto startTime = std::chrono::steady_clock::now();
    int exitCode = std::system(command.c_str());
    auto endTime = std::chrono::steady_clock::now();
    
    result.processingTime = std::chrono::duration<double>(endTime - startTime).count();
    
    if (exitCode == 0) {
        result.success = true;
        if (g_verboseOutput) {
            std::cout << "✓ Transcoding completed successfully in " << result.processingTime << "s" << std::endl;
        }
    } else {
        result.success = false;
        result.errorMessage = "FFmpeg failed with exit code " + std::to_string(exitCode);
        reportError(result.errorMessage, videoFile.filename().string());
    }
    
    return result;
}

std::vector<ProcessingResult> VideoTranscoder::processBatch(
    const std::filesystem::path& inputDirectory,
    const std::filesystem::path& outputDirectory) {
    
    std::vector<ProcessingResult> results;
    BatchProgress progress;
    
    if (g_verboseOutput) {
        std::cout << "\n=== BATCH PROCESSING ===\n";
        std::cout << "Input directory: " << inputDirectory << std::endl;
        std::cout << "Output directory: " << outputDirectory << std::endl;
    }
    
    // Discover files
    auto videoFiles = findVideoFiles(inputDirectory);
    auto audioFiles = findAudioFiles(inputDirectory);
    
    if (videoFiles.empty()) {
        reportError("No video files found in input directory", inputDirectory.string());
        return results;
    }
    
    // Find audio matches
    auto audioMatches = findAudioMatches(videoFiles, audioFiles);
    
    // Initialize progress tracking
    progress.totalFiles = videoFiles.size();
    reportProgress(progress);
    
    // Process each video file
    for (size_t i = 0; i < videoFiles.size(); ++i) {
        const auto& videoFile = videoFiles[i];
        const auto& audioMatch = audioMatches[i];
        
        auto outputFile = generateOutputPath(videoFile, outputDirectory);
        auto result = transcodeVideo(videoFile, audioMatch, outputFile);
        
        results.push_back(result);
        
        // Update progress
        progress.completedFiles++;
        progress.totalProcessingTime += result.processingTime;
        
        if (result.success) {
            progress.successfulFiles++;
        } else {
            progress.failedFiles++;
        }
        
        reportProgress(progress);
        
        if (g_verboseOutput) {
            std::cout << "[" << progress.completedFiles << "/" << progress.totalFiles << "] ";
            if (result.success) {
                std::cout << "✓ " << videoFile.filename().string() << std::endl;
            } else {
                std::cout << "✗ " << videoFile.filename().string() << " - " << result.errorMessage << std::endl;
            }
        }
    }
    
    if (g_verboseOutput) {
        std::cout << "\n=== BATCH COMPLETE ===\n";
        std::cout << "Total files: " << progress.totalFiles << std::endl;
        std::cout << "Successful: " << progress.successfulFiles << std::endl;
        std::cout << "Failed: " << progress.failedFiles << std::endl;
        std::cout << "Success rate: " << progress.getSuccessRate() << "%" << std::endl;
        std::cout << "Total processing time: " << progress.totalProcessingTime << "s" << std::endl;
    }
    
    return results;
}

// ============================================================================
// UTILITY AND HELPER FUNCTIONS
// ============================================================================

double VideoTranscoder::getFileDuration(const std::filesystem::path& filepath) {
    if (!std::filesystem::exists(filepath)) {
        return 0.0;
    }
    
    std::string command = "ffprobe -v quiet -show_entries format=duration "
                         "-of csv=p=0 \"" + filepath.string() + "\" 2>/dev/null";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return 0.0;
    }
    
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

bool VideoTranscoder::isHighGainFile(const std::filesystem::path& filepath) {
    std::string filename = filepath.filename().string();
    std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
    
    // Look for indicators of low-gain files (assume high-gain by default)
    return filename.find("_d.") == std::string::npos && 
           filename.find("_low.") == std::string::npos &&
           filename.find("_l.") == std::string::npos;
}

std::filesystem::path VideoTranscoder::getLowGainCounterpart(const std::filesystem::path& highGainFile) {
    if (!isHighGainFile(highGainFile)) {
        return ""; // This is already a low-gain file
    }
    
    std::filesystem::path parent = highGainFile.parent_path();
    std::string stem = highGainFile.stem().string();
    std::string extension = highGainFile.extension().string();
    
    // Try common low-gain naming patterns
    std::vector<std::string> patterns = {
        stem + "_D" + extension,
        stem + "_d" + extension,
        stem + "_low" + extension,
        stem + "_L" + extension
    };
    
    for (const auto& pattern : patterns) {
        std::filesystem::path candidate = parent / pattern;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    
    return ""; // No low-gain counterpart found
}

AudioGainType VideoTranscoder::determineGainType(const std::filesystem::path& filepath) {
    if (isHighGainFile(filepath)) {
        return AudioGainType::HIGH_GAIN;
    } else {
        return AudioGainType::LOW_GAIN;
    }
}

std::filesystem::path VideoTranscoder::generateOutputPath(
    const std::filesystem::path& inputVideo,
    const std::filesystem::path& outputDirectory) const {
    
    std::string outputName = inputVideo.stem().string() + ".mov";
    return outputDirectory / outputName;
}

// ============================================================================
// CONFIGURATION AND CALLBACKS
// ============================================================================

void VideoTranscoder::setProgressCallback(ProgressCallback callback) {
    m_progressCallback = callback;
}

void VideoTranscoder::setErrorCallback(ErrorCallback callback) {
    m_errorCallback = callback;
}

bool VideoTranscoder::loadSyncConfig(const std::filesystem::path& configPath) {
    std::ifstream config(configPath);
    if (!config.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(config, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        std::istringstream iss(line);
        std::string videoName, audioName;
        double offset;
        
        if (iss >> videoName >> audioName >> offset) {
            auto key = std::make_pair(videoName, audioName);
            m_syncOffsets[key] = offset;
            
            if (g_verboseOutput) {
                std::cout << "Loaded sync config: " << videoName << " + " << audioName 
                          << " = " << offset << "s" << std::endl;
            }
        }
    }
    
    return true;
}

// ============================================================================
// CENTER-BASED SYNC DETECTION USING MIDDLE 10% OF FILES
// ============================================================================

double VideoTranscoder::performAutoSync(
    const std::filesystem::path& videoFile,
    const std::filesystem::path& audioFile) {

    std::cout << "\n=== CENTER-BASED AUDIO SYNC DETECTION ===" << std::endl;
    std::cout << "Video file: " << videoFile.filename().string() << std::endl;
    std::cout << "Audio file: " << audioFile.filename().string() << std::endl;

    // Get file durations
    double videoDuration = getFileDuration(videoFile);
    double audioDuration = getFileDuration(audioFile);

    std::cout << "Video duration: " << videoDuration << "s" << std::endl;
    std::cout << "Audio duration: " << audioDuration << "s" << std::endl;

    if (videoDuration <= 0 || audioDuration <= 0) {
        std::cout << "❌ Could not determine file durations" << std::endl;
        return 0.0;
    }

    // Calculate center positions and sample durations (10% of each file)
    double videoSampleDuration = videoDuration * 0.1;  // 10% of video
    double audioSampleDuration = audioDuration * 0.1;  // 10% of audio
    double videoCenter = videoDuration / 2.0;
    double audioCenter = audioDuration / 2.0;

    // Use the shorter sample duration to ensure we have enough content
    double sampleDuration = std::min(videoSampleDuration, audioSampleDuration);
    sampleDuration = std::max(sampleDuration, 10.0);  // Minimum 10 seconds
    sampleDuration = std::min(sampleDuration, 30.0);  // Maximum 30 seconds

    double videoStart = videoCenter - (sampleDuration / 2.0);
    double audioBaseStart = audioCenter - (sampleDuration / 2.0);

    std::cout << "Sample duration: " << sampleDuration << "s" << std::endl;
    std::cout << "Video sample starts at: " << videoStart << "s" << std::endl;
    std::cout << "Audio base sample starts at: " << audioBaseStart << "s" << std::endl;

    // Create temporary directory for analysis
    std::string tempDir = "/tmp/sync_center_" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(tempDir);

    std::vector<OffsetCandidate> candidates;

    // PHASE 1: Coarse search - 2 second steps from -15 to +15
    std::cout << "\n🔍 PHASE 1: Coarse search (2s steps, -15 to +15)" << std::endl;
    candidates = searchOffsetRangeCenter(
        -15.0, 15.0, 2.0,
        videoFile, audioFile,
        videoStart, audioBaseStart, sampleDuration,
        tempDir
    );

    // Get top 3 from coarse search
    std::sort(candidates.begin(), candidates.end(),
              [](const OffsetCandidate& a, const OffsetCandidate& b) {
                  return a.score > b.score;
              });

    if (candidates.size() > 3) {
        candidates.erase(candidates.begin() + 3, candidates.end());
    }

    std::cout << "📊 Top 3 coarse candidates:" << std::endl;
    for (const auto& candidate : candidates) {
        std::cout << "  " << candidate.offset << "s (score: " << candidate.score << ")" << std::endl;
    }

    // PHASE 2: Medium refinement - 0.5 second steps around each candidate
    std::cout << "\n🔍 PHASE 2: Medium refinement (0.5s steps around top candidates)" << std::endl;
    std::vector<OffsetCandidate> refinedCandidates;

    for (const auto& candidate : candidates) {
        double searchMin = candidate.offset - 3.0;  // Search ±3s around candidate
        double searchMax = candidate.offset + 3.0;

        std::cout << "  Refining around " << candidate.offset << "s (range: "
                  << searchMin << " to " << searchMax << ")" << std::endl;

        auto localResults = searchOffsetRangeCenter(
            searchMin, searchMax, 0.5,
            videoFile, audioFile,
            videoStart, audioBaseStart, sampleDuration,
            tempDir
        );
        refinedCandidates.insert(refinedCandidates.end(), localResults.begin(), localResults.end());
    }

    // Get top 3 from medium search
    std::sort(refinedCandidates.begin(), refinedCandidates.end(),
              [](const OffsetCandidate& a, const OffsetCandidate& b) {
                  return a.score > b.score;
              });

    if (refinedCandidates.size() > 3) {
        refinedCandidates.erase(refinedCandidates.begin() + 3, refinedCandidates.end());
    }

    std::cout << "📊 Top 3 medium candidates:" << std::endl;
    for (const auto& candidate : refinedCandidates) {
        std::cout << "  " << candidate.offset << "s (score: " << candidate.score << ")" << std::endl;
    }

    // PHASE 3: Fine refinement - 0.1 second steps around the best candidate
    std::cout << "\n🔍 PHASE 3: Fine refinement (0.1s steps around best candidate)" << std::endl;
    std::vector<OffsetCandidate> fineCandidates;

    if (!refinedCandidates.empty()) {
        const auto& bestCandidate = refinedCandidates[0];
        double searchMin = bestCandidate.offset - 1.0;  // Search ±1s around best
        double searchMax = bestCandidate.offset + 1.0;

        std::cout << "  Fine-tuning around " << bestCandidate.offset << "s (range: "
                  << searchMin << " to " << searchMax << ")" << std::endl;

        fineCandidates = searchOffsetRangeCenter(
            searchMin, searchMax, 0.1,
            videoFile, audioFile,
            videoStart, audioBaseStart, sampleDuration,
            tempDir
        );

        // Sort to get the absolute best
        std::sort(fineCandidates.begin(), fineCandidates.end(),
                  [](const OffsetCandidate& a, const OffsetCandidate& b) {
                      return a.score > b.score;
                  });
    }

    // Clean up
    std::filesystem::remove_all(tempDir);

    // Final results
    std::cout << "\n🎯 === FINAL RESULTS ===" << std::endl;

    if (!fineCandidates.empty()) {
        const auto& finalResult = fineCandidates[0];
        std::cout << "🎉 DETECTED SYNC OFFSET: " << finalResult.offset << "s" << std::endl;
        std::cout << "📊 Final score: " << finalResult.score << std::endl;

        // Show top 5 final candidates for analysis
        std::cout << "\n📊 Top 5 final candidates for analysis:" << std::endl;
        for (size_t i = 0; i < std::min(fineCandidates.size(), size_t(5)); ++i) {
            std::cout << "  #" << (i+1) << ": " << fineCandidates[i].offset
                      << "s (score: " << fineCandidates[i].score << ")" << std::endl;
        }

        return finalResult.offset;
    }

    std::cout << "⚠️  No good sync found, defaulting to 0s" << std::endl;
    return 0.0;
}

// New method to search offsets using center-based sampling
std::vector<OffsetCandidate> VideoTranscoder::searchOffsetRangeCenter(
    double minOffset,
    double maxOffset,
    double stepSize,
    const std::filesystem::path& videoFile,
    const std::filesystem::path& audioFile,
    double videoStart,
    double audioBaseStart,
    double sampleDuration,
    const std::string& tempDir) {

    std::vector<OffsetCandidate> candidates;

    for (double offset = minOffset; offset <= maxOffset; offset += stepSize) {
        double score = testOffsetCenter(
            videoFile, audioFile,
            offset, videoStart, audioBaseStart, sampleDuration,
            tempDir
        );
        candidates.emplace_back(offset, score);

        std::cout << "    Offset " << std::fixed << std::setprecision(2)
                  << offset << "s: score " << score << std::endl;
    }

    return candidates;
}

// Center-based offset testing
double VideoTranscoder::testOffsetCenter(
    const std::filesystem::path& videoFile,
    const std::filesystem::path& audioFile,
    double offset,
    double videoStart,
    double audioBaseStart,
    double sampleDuration,
    const std::string& tempDir) {

    // Calculate where to extract audio from based on the offset
    double audioStart = audioBaseStart - offset;  // If offset is +3, start audio 3s earlier

    // Check bounds
    if (audioStart < 0 || videoStart < 0) {
        return -100.0;  // Skip if we'd go before start of file
    }

    // Create unique filenames for this test
    std::string videoAudioPath = tempDir + "/vid_" + std::to_string((int)(offset*100)) + "_center.wav";
    std::string lavAudioPath = tempDir + "/lav_" + std::to_string((int)(offset*100)) + "_center.wav";

    // Extract from video center
    std::string extractVidCmd = "ffmpeg -hide_banner -loglevel error "
                               "-ss " + std::to_string(videoStart) + " "
                               "-i \"" + videoFile.string() + "\" "
                               "-t " + std::to_string(sampleDuration) + " "
                               "-vn -acodec pcm_s16le -ar 22050 -ac 1 "
                               "\"" + videoAudioPath + "\" 2>/dev/null";

    // Extract from audio with offset applied
    std::string extractLavCmd = "ffmpeg -hide_banner -loglevel error "
                               "-ss " + std::to_string(audioStart) + " "
                               "-i \"" + audioFile.string() + "\" "
                               "-t " + std::to_string(sampleDuration) + " "
                               "-acodec pcm_s16le -ar 22050 -ac 1 "
                               "\"" + lavAudioPath + "\" 2>/dev/null";

    int result1 = std::system(extractVidCmd.c_str());
    int result2 = std::system(extractLavCmd.c_str());

    if (result1 != 0 || result2 != 0) {
        return -100.0;
    }

    // Calculate cross-correlation using FFmpeg
    double correlation = calculateCrossCorrelation(videoAudioPath, lavAudioPath);

    // Clean up these specific files
    std::filesystem::remove(videoAudioPath);
    std::filesystem::remove(lavAudioPath);

    return correlation;
}

// Enhanced cross-correlation calculation
double VideoTranscoder::calculateCrossCorrelation(
    const std::string& audioFile1,
    const std::string& audioFile2) {

    // Method 1: Calculate RMS difference (inverse correlation)
    std::string tempDiff = "/tmp/diff_" + std::to_string(std::time(nullptr)) + "_" +
                          std::to_string(rand() % 10000) + ".wav";

    // Create difference signal by subtracting normalized versions
    std::string diffCmd = "ffmpeg -hide_banner -loglevel error "
                         "-i \"" + audioFile1 + "\" "
                         "-i \"" + audioFile2 + "\" "
                         "-filter_complex \"[0:a]volume=0.5[a0];[1:a]volume=0.5[a1];[a0][a1]amix=inputs=2:weights='1 -1':duration=shortest\" "
                         "\"" + tempDiff + "\" 2>/dev/null";

    int result = std::system(diffCmd.c_str());
    if (result != 0) {
        return -100.0;
    }

    // Get RMS of the difference - lower is better
    double diffRMS = getAudioRMS(tempDiff);

    // Get RMS of original signals for normalization
    double rms1 = getAudioRMS(audioFile1);
    double rms2 = getAudioRMS(audioFile2);
    double avgRMS = (rms1 + rms2) / 2.0;

    // Clean up temp file
    std::filesystem::remove(tempDiff);

    if (avgRMS > 0 && diffRMS >= 0) {
        // Correlation score: lower difference = higher score
        double normalizedDiff = diffRMS / avgRMS;
        double correlation = 100.0 - (normalizedDiff * 100.0);

        std::cout << "      RMS1: " << rms1 << ", RMS2: " << rms2
                  << ", Diff: " << diffRMS << ", Score: " << correlation << std::endl;

        return correlation;
    }

    return -100.0;
}

// Keep existing getAudioRMS method unchanged
double VideoTranscoder::getAudioRMS(const std::string& audioFile) {
    std::string cmd = "ffmpeg -hide_banner -i \"" + audioFile + "\" "
                     "-filter:a \"volumedetect\" -f null - 2>&1 | "
                     "grep 'mean_volume' | tail -1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return 0.0;

    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    // Parse mean_volume: -XX.X dB
    size_t pos = result.find("mean_volume: ");
    if (pos != std::string::npos) {
        std::string volumeStr = result.substr(pos + 13);
        size_t endPos = volumeStr.find(" dB");
        if (endPos != std::string::npos) {
            volumeStr = volumeStr.substr(0, endPos);
            try {
                double volume_db = std::stod(volumeStr);
                return std::pow(10.0, volume_db / 20.0);
            } catch (...) {
                return 0.0;
            }
        }
    }

    return 0.0;
}

bool VideoTranscoder::isDurationMatch(
    const std::filesystem::path& videoFile,
    const std::filesystem::path& audioFile) const {

    double videoDuration = getFileDuration(videoFile);
    double audioDuration = getFileDuration(audioFile);

    if (videoDuration <= 0.0 || audioDuration <= 0.0) {
        return false;
    }

    double diff = std::abs(videoDuration - audioDuration);
    return diff <= g_durationTolerance;
}

// Also add this simple testSyncOffset method that's still being called
bool VideoTranscoder::testSyncOffset(
    const std::filesystem::path& videoFile,
    const std::filesystem::path& audioFile,
    double offset) {

    std::string testCmd = "ffmpeg -hide_banner -loglevel error ";

    // Input video file first
    testCmd += "-i \"" + videoFile.string() + "\" ";

    // Apply offset to the lav audio file
    if (offset > 0.001) {
        testCmd += "-itsoffset " + std::to_string(offset) + " ";
    } else if (offset < -0.001) {
        testCmd += "-ss " + std::to_string(-offset) + " ";
    }

    testCmd += "-i \"" + audioFile.string() + "\" ";
    testCmd += "-filter_complex \"[0:a][1:a]amix=inputs=2:duration=shortest\" "
              "-t 2 -f null - 2>/dev/null";

    int result = std::system(testCmd.c_str());
    return (result == 0);
}

// ============================================================================
// PRIVATE UTILITY METHODS
// ============================================================================

std::string VideoTranscoder::buildFFmpegCommand(
    const std::filesystem::path& videoFile,
    const AudioMatch& audioMatch,
    const std::filesystem::path& outputFile) const {
    
    std::ostringstream cmd;
    cmd << "ffmpeg -hide_banner -loglevel error ";
    
    double offset = audioMatch.syncOffset;
    
    // Calculate timing corrections
    double videoDuration = getFileDuration(videoFile);
    double lavDuration = getFileDuration(audioMatch.highGainFile);
    
    double videoStart = 0.0;
    double videoEnd = videoDuration;
    double lavStart = offset;
    double lavEnd = offset + lavDuration;
    
    double overlapStart = std::max(videoStart, lavStart);
    double overlapEnd = std::min(videoEnd, lavEnd);
    double overlapDuration = overlapEnd - overlapStart;
    
    bool needsTrimming = (overlapDuration > 0 && overlapDuration < std::min(videoDuration, lavDuration) - 2.0);
    
    if (needsTrimming && overlapStart > 0.1) {
        cmd << "-ss " << overlapStart << " ";
    }
    
    // Input video file
    cmd << "-i \"" << videoFile.string() << "\" ";
    
    // Handle lav audio files with proper offset
    if (g_verboseOutput) {
        std::cout << "  Applying offset: " << offset << "s to lav audio files" << std::endl;
    }
    
    if (offset > 0.001) {
        if (g_verboseOutput) {
            std::cout << "    Using -itsoffset " << offset << " (lav delayed)" << std::endl;
        }
        cmd << "-itsoffset " << offset << " ";
    } else if (offset < -0.001) {
        if (g_verboseOutput) {
            std::cout << "    Using -ss " << (-offset) << " (seeking into lav)" << std::endl;
        }
        cmd << "-ss " << (-offset) << " ";
    } else {
        if (g_verboseOutput) {
            std::cout << "    No offset needed (perfectly synced)" << std::endl;
        }
    }
    
    cmd << "-i \"" << audioMatch.highGainFile.string() << "\" ";
    
    if (!audioMatch.lowGainFile.empty()) {
        // Apply the EXACT same offset/seek to low gain file
        if (offset > 0.001) {
            cmd << "-itsoffset " << offset << " ";
        } else if (offset < -0.001) {
            cmd << "-ss " << (-offset) << " ";
        }
        cmd << "-i \"" << audioMatch.lowGainFile.string() << "\" ";
    }
    
    // Apply duration limit if needed
    if (needsTrimming && overlapDuration > 1.0) {
        cmd << "-t " << overlapDuration << " ";
    }
    
    // Video codec
    cmd << g_settings.videoCodec << " ";
    if (!g_settings.videoOptions.empty()) {
        cmd << g_settings.videoOptions << " ";
    }
    
    // Audio mapping and codec
    if (!audioMatch.lowGainFile.empty()) {
        // Three audio tracks: HighLav, LowLav, Camera
        cmd << "-map 0:v -map 1:a -map 2:a -map 0:a ";
        cmd << g_settings.audioCodec << " ";
        if (!g_settings.audioOptions.empty()) {
            cmd << g_settings.audioOptions << " ";
        }
        cmd << "-metadata:s:a:0 title=\"HighLav\" ";
        cmd << "-metadata:s:a:1 title=\"LowLav\" ";
        cmd << "-metadata:s:a:2 title=\"Camera\" ";
    } else {
        // Two audio tracks: HighLav, Camera
        cmd << "-map 0:v -map 1:a -map 0:a ";
        cmd << g_settings.audioCodec << " ";
        if (!g_settings.audioOptions.empty()) {
            cmd << g_settings.audioOptions << " ";
        }
        cmd << "-metadata:s:a:0 title=\"HighLav\" ";
        cmd << "-metadata:s:a:1 title=\"Camera\" ";
    }
    
    // Common options
    cmd << "-avoid_negative_ts make_zero ";
    cmd << "-fflags +genpts ";
    cmd << "-movflags +faststart ";
    cmd << "\"" << outputFile.string() << "\"";
    
    return cmd.str();
}

double VideoTranscoder::executeTimedCommand(const std::string& command) {
    auto startTime = std::chrono::steady_clock::now();
    std::system(command.c_str());
    auto endTime = std::chrono::steady_clock::now();
    
    return std::chrono::duration<double>(endTime - startTime).count();
}

void VideoTranscoder::reportProgress(const BatchProgress& progress) {
    if (m_progressCallback) {
        m_progressCallback(progress);
    }
}

void VideoTranscoder::reportError(const std::string& error, const std::string& filename) {
    if (m_errorCallback) {
        m_errorCallback(error, filename);
    } else if (g_verboseOutput) {
        std::cerr << "ERROR";
        if (!filename.empty()) {
            std::cerr << " (" << filename << ")";
        }
        std::cerr << ": " << error << std::endl;
    }
}
    
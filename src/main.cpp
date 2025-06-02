/**
 * @file main.cpp
 * @brief Main entry point for the video transcoder application
 * @author Video Transcoder
 * @date 2025
 * 
 * This file contains the main application loop, command line argument parsing,
 * and high-level coordination of the transcoding process.
 */

#include "globals.h"
#include "transcoder.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void printUsage(const char* programName);
void printVersion();
bool parseCommandLineArgs(int argc, char* argv[], std::string& inputDir, std::string& outputDir);
void setupCallbacks(VideoTranscoder& transcoder);
void progressCallback(const BatchProgress& progress);
void errorCallback(const std::string& error, const std::string& filename);
void printSummary(const std::vector<ProcessingResult>& results, double totalTime);

// ============================================================================
// MAIN APPLICATION ENTRY POINT
// ============================================================================

/**
 * @brief Main entry point for the video transcoder application
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return Exit code (0 = success, non-zero = error)
 */
int main(int argc, char* argv[]) {
    // Display application header
    std::cout << "Video Transcoder with Audio Synchronization v1.0\n";
    std::cout << "================================================\n\n";
    
    // Parse command line arguments
    const std::string inputDirectory = "/s3/";
    const std::string outputDirectory = "/s3/output/";
//   if (!parseCommandLineArgs(argc, argv, inputDirectory, outputDirectory)) {
//       return 1;
//   }
    
    // Validate input directory
    if (!std::filesystem::exists(inputDirectory)) {
        std::cerr << "ERROR: Input directory does not exist: " << inputDirectory << std::endl;
        return 1;
    }
    
    if (!std::filesystem::is_directory(inputDirectory)) {
        std::cerr << "ERROR: Input path is not a directory: " << inputDirectory << std::endl;
        return 1;
    }
    
    // Create output directory if it doesn't exist
    try {
        if (!std::filesystem::exists(outputDirectory)) {
            std::filesystem::create_directories(outputDirectory);
            if (g_verboseOutput) {
                std::cout << "Created output directory: " << outputDirectory << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to create output directory: " << e.what() << std::endl;
        return 1;
    }
    
    // Initialize transcoder
    VideoTranscoder transcoder;
    setupCallbacks(transcoder);
    
    // Load sync configuration if specified
    if (!g_syncConfigFile.empty()) {
        if (transcoder.loadSyncConfig(g_syncConfigFile)) {
            if (g_verboseOutput) {
                std::cout << "Loaded sync configuration from: " << g_syncConfigFile << std::endl;
            }
        } else {
            std::cerr << "WARNING: Failed to load sync config file: " << g_syncConfigFile << std::endl;
        }
    }
    
    // Record start time for total processing measurement
    auto startTime = std::chrono::steady_clock::now();
    
    // Process the batch
    std::cout << "Processing files from: " << inputDirectory << std::endl;
    std::cout << "Output directory: " << outputDirectory << std::endl;
    
    if (g_dryRun) {
        std::cout << "\n*** DRY RUN MODE - No files will be processed ***\n" << std::endl;
    }
    
    std::vector<ProcessingResult> results = transcoder.processBatch(inputDirectory, outputDirectory);
    
    // Calculate total time
    auto endTime = std::chrono::steady_clock::now();
    double totalTime = std::chrono::duration<double>(endTime - startTime).count();
    
    // Print final summary
    printSummary(results, totalTime);
    
    // Return appropriate exit code
    size_t failedCount = 0;
    for (const auto& result : results) {
        if (!result.success) {
            failedCount++;
        }
    }
    
    return (failedCount == 0) ? 0 : 1;
}

// ============================================================================
// COMMAND LINE ARGUMENT PARSING
// ============================================================================

/**
 * @brief Parse command line arguments and set global configuration
 * @param argc Number of arguments
 * @param argv Argument array
 * @param inputDir [out] Parsed input directory
 * @param outputDir [out] Parsed output directory
 * @return true if parsing successful, false if should exit
 */
bool parseCommandLineArgs(int argc, char* argv[], std::string& inputDir, std::string& outputDir) {
    if (argc < 3) {
        printUsage(argv[0]);
        return false;
    }
    
    // Parse flags and options
    for (int i = 1; i < argc - 2; ++i) {
        if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--verbose") == 0) {
            g_verboseOutput = true;
        }
        else if (std::strcmp(argv[i], "-n") == 0 || std::strcmp(argv[i], "--dry-run") == 0) {
            g_dryRun = true;
        }
        else if (std::strcmp(argv[i], "--version") == 0) {
            printVersion();
            return false;
        }
        else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return false;
        }
        else if (std::strcmp(argv[i], "-t") == 0 || std::strcmp(argv[i], "--tolerance") == 0) {
            if (i + 1 < argc - 2) {
                try {
                    g_durationTolerance = std::stod(argv[++i]);
                } catch (...) {
                    std::cerr << "ERROR: Invalid tolerance value: " << argv[i] << std::endl;
                    return false;
                }
            } else {
                std::cerr << "ERROR: --tolerance requires a value" << std::endl;
                return false;
            }
        }
        else if (std::strcmp(argv[i], "-c") == 0 || std::strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc - 2) {
                g_syncConfigFile = argv[++i];
            } else {
                std::cerr << "ERROR: --config requires a file path" << std::endl;
                return false;
            }
        }
        else if (std::strcmp(argv[i], "--codec") == 0) {
            if (i + 1 < argc - 2) {
                g_settings.videoCodec = argv[++i];
            } else {
                std::cerr << "ERROR: --codec requires a codec string" << std::endl;
                return false;
            }
        }
        else if (std::strcmp(argv[i], "--quality") == 0) {
            if (i + 1 < argc - 2) {
                std::string quality = argv[++i];
                if (quality == "proxy-low") {
                    g_settings.quality = TranscodeQuality::PROXY_LOW;
                    g_settings.videoCodec = "-c:v libx264";
                    g_settings.videoOptions = "-preset ultrafast -crf 28";
                } else if (quality == "proxy-medium") {
                    g_settings.quality = TranscodeQuality::PROXY_MEDIUM;
                    g_settings.videoCodec = "-c:v mjpeg";
                    g_settings.videoOptions = "-q:v 2";
                } else if (quality == "proxy-high") {
                    g_settings.quality = TranscodeQuality::PROXY_HIGH;
                    g_settings.videoCodec = "-c:v libx264";
                    g_settings.videoOptions = "-preset fast -crf 18";
                } else if (quality == "production") {
                    g_settings.quality = TranscodeQuality::PRODUCTION;
                    g_settings.videoCodec = "-c:v libx264";
                    g_settings.videoOptions = "-preset slow -crf 15";
                } else if (quality == "archive") {
                    g_settings.quality = TranscodeQuality::ARCHIVE;
                    g_settings.videoCodec = "-c:v libx264";
                    g_settings.videoOptions = "-preset veryslow -crf 12";
                } else {
                    std::cerr << "ERROR: Unknown quality preset: " << quality << std::endl;
                    return false;
                }
            } else {
                std::cerr << "ERROR: --quality requires a preset name" << std::endl;
                return false;
            }
        }
        else {
            std::cerr << "ERROR: Unknown option: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return false;
        }
    }
    
    // Get input and output directories (last two arguments)
    inputDir = argv[argc - 2];
    outputDir = argv[argc - 1];
    
    return true;
}

/**
 * @brief Print usage information
 * @param programName Name of the program executable
 */
void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS] INPUT_DIR OUTPUT_DIR\n\n";
    
    std::cout << "OPTIONS:\n";
    std::cout << "  -v, --verbose         Enable verbose output\n";
    std::cout << "  -n, --dry-run         Show what would be done without processing\n";
    std::cout << "  -t, --tolerance SEC   Duration tolerance for matching (default: " 
              << DEFAULT_DURATION_TOLERANCE << "s)\n";
    std::cout << "  -c, --config FILE     Load sync offsets from configuration file\n";
    std::cout << "  --codec CODEC         Override video codec (e.g., '-c:v libx264')\n";
    std::cout << "  --quality PRESET      Quality preset: proxy-low, proxy-medium, proxy-high,\n";
    std::cout << "                        production, archive (default: proxy-medium)\n";
    std::cout << "  -h, --help            Show this help message\n";
    std::cout << "  --version             Show version information\n\n";
    
    std::cout << "EXAMPLES:\n";
    std::cout << "  " << programName << " ./input ./output\n";
    std::cout << "  " << programName << " -v --quality production ./raw ./processed\n";
    std::cout << "  " << programName << " --tolerance 60 --config sync.txt ./src ./dst\n\n";
    
    std::cout << "SYNC CONFIG FILE FORMAT:\n";
    std::cout << "  video_file.mp4 audio_file.wav offset_seconds\n";
    std::cout << "  C0001.MP4 001_250601.wav -2.5\n";
    std::cout << "  C0002.MP4 002_250601.wav 1.3\n";
}

/**
 * @brief Print version information
 */
void printVersion() {
    std::cout << "Video Transcoder v1.0\n";
    std::cout << "Built with FFmpeg integration\n";
    std::cout << "Supports automatic audio-video synchronization\n";
}

// ============================================================================
// CALLBACK SETUP AND IMPLEMENTATIONS
// ============================================================================

/**
 * @brief Set up progress and error callbacks for the transcoder
 * @param transcoder Transcoder instance to configure
 */
void setupCallbacks(VideoTranscoder& transcoder) {
    transcoder.setProgressCallback(progressCallback);
    transcoder.setErrorCallback(errorCallback);
}

/**
 * @brief Progress callback implementation
 * @param progress Current batch progress
 */
void progressCallback(const BatchProgress& progress) {
    if (!g_verboseOutput) {
        // Show simple progress bar for non-verbose mode
        double percentage = progress.getCompletionPercentage();
        int barWidth = 50;
        int filledWidth = static_cast<int>(percentage / 100.0 * barWidth);
        
        std::cout << "\rProgress: [";
        for (int i = 0; i < barWidth; ++i) {
            if (i < filledWidth) {
                std::cout << "=";
            } else if (i == filledWidth) {
                std::cout << ">";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "] " << std::fixed << std::setprecision(1) << percentage << "% ";
        std::cout << "(" << progress.completedFiles << "/" << progress.totalFiles << ")";
        std::cout.flush();
        
        if (progress.completedFiles == progress.totalFiles) {
            std::cout << std::endl; // New line when complete
        }
    }
}

/**
 * @brief Error callback implementation
 * @param error Error message
 * @param filename File that caused the error
 */
void errorCallback(const std::string& error, const std::string& filename) {
    std::cerr << "ERROR";
    if (!filename.empty()) {
        std::cerr << " (" << filename << ")";
    }
    std::cerr << ": " << error << std::endl;
}

// ============================================================================
// SUMMARY AND REPORTING
// ============================================================================

/**
 * @brief Print final processing summary
 * @param results Vector of all processing results
 * @param totalTime Total elapsed time
 */
void printSummary(const std::vector<ProcessingResult>& results, double totalTime) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "PROCESSING SUMMARY" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    size_t totalFiles = results.size();
    size_t successfulFiles = 0;
    size_t failedFiles = 0;
    double totalProcessingTime = 0.0;
    
    for (const auto& result : results) {
        if (result.success) {
            successfulFiles++;
        } else {
            failedFiles++;
        }
        totalProcessingTime += result.processingTime;
    }
    
    std::cout << "Total files processed: " << totalFiles << std::endl;
    std::cout << "Successful: " << successfulFiles << std::endl;
    std::cout << "Failed: " << failedFiles << std::endl;
    
    if (totalFiles > 0) {
        double successRate = (static_cast<double>(successfulFiles) / totalFiles) * 100.0;
        std::cout << "Success rate: " << std::fixed << std::setprecision(1) << successRate << "%" << std::endl;
    }
    
    std::cout << "Total wall time: " << std::fixed << std::setprecision(1) << totalTime << "s" << std::endl;
    std::cout << "Total processing time: " << std::fixed << std::setprecision(1) << totalProcessingTime << "s" << std::endl;
    
    if (totalFiles > 0) {
        double avgTime = totalProcessingTime / totalFiles;
        std::cout << "Average time per file: " << std::fixed << std::setprecision(1) << avgTime << "s" << std::endl;
    }
    
    // Show failed files if any
    if (failedFiles > 0) {
        std::cout << "\nFAILED FILES:" << std::endl;
        for (const auto& result : results) {
            if (!result.success) {
                std::cout << "  " << result.inputVideo.filename().string() 
                          << " - " << result.errorMessage << std::endl;
            }
        }
    }
    
    // Show sync information in verbose mode
    if (g_verboseOutput && successfulFiles > 0) {
        std::cout << "\nSYNC INFORMATION:" << std::endl;
        for (const auto& result : results) {
            if (result.success && result.audioMatch.isValid()) {
                std::cout << "  " << result.inputVideo.filename().string() 
                          << " + " << result.audioMatch.highGainFile.filename().string()
                          << " (offset: " << std::fixed << std::setprecision(3) 
                          << result.audioMatch.syncOffset << "s)" << std::endl;
            }
        }
    }
    
    std::cout << std::string(60, '=') << std::endl;
    
    if (g_dryRun) {
        std::cout << "DRY RUN COMPLETE - No files were actually processed." << std::endl;
    } else if (failedFiles == 0 && totalFiles > 0) {
        std::cout << "All files processed successfully!" << std::endl;
    } else if (failedFiles > 0) {
        std::cout << "Processing completed with " << failedFiles << " failures." << std::endl;
    } else {
        std::cout << "No files found to process." << std::endl;
    }
}
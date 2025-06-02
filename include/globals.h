/**
 * @file globals.h
 * @brief Global definitions, data structures, enums, and shared variables
 * @author Video Transcoder
 * @date 2025
 *
 * This file contains all shared data structures, enums, constants, and global
 * variables that need to be accessed across multiple modules of the transcoder.
 */
#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <optional>

// ============================================================================
// CONSTANTS AND CONFIGURATION
// ============================================================================

/// @brief Default duration tolerance for audio-video matching (seconds)
constexpr double DEFAULT_DURATION_TOLERANCE = 30.0;

/// @brief Default sync detection timeout (seconds)
constexpr double DEFAULT_SYNC_TIMEOUT = 60.0;

/// @brief Maximum offset range for sync detection (seconds)
constexpr double MAX_SYNC_OFFSET = 30.0;

/// @brief Minimum confidence threshold for sync matches
constexpr double MIN_SYNC_CONFIDENCE = 0.5;

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum TranscodeQuality
 * @brief Quality presets for video transcoding
 */
enum class TranscodeQuality {
    PROXY_LOW,      ///< Low quality for proxies (fast)
    PROXY_MEDIUM,   ///< Medium quality for proxies
    PROXY_HIGH,     ///< High quality for proxies
    PRODUCTION,     ///< Production quality (slow, high quality)
    ARCHIVE         ///< Archive quality (highest quality, very slow)
};

/**
 * @enum AudioGainType
 * @brief Type of audio gain level from lavalier microphones
 */
enum class AudioGainType {
    HIGH_GAIN,      ///< High gain audio (typically louder, may clip)
    LOW_GAIN,       ///< Low gain audio (typically quieter, cleaner)
    UNKNOWN         ///< Gain type could not be determined
};

/**
 * @enum SyncMethod
 * @brief Method used for audio-video synchronization
 */
enum class SyncMethod {
    DURATION_MATCH, ///< Match by duration similarity only
    AUTO_CORRELATION, ///< Automatic correlation-based sync
    MANUAL_OFFSET,  ///< Manually specified offset
    CONFIG_FILE     ///< Offset from configuration file
};

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * @struct TranscodeSettings
 * @brief Transcoding parameters and quality settings
 */
struct TranscodeSettings {
    std::string videoCodec;         ///< Video codec string (e.g., "-c:v libx264")
    std::string videoOptions;       ///< Additional video options
    std::string audioCodec;         ///< Audio codec string (e.g., "-c:a pcm_s16le")
    std::string audioOptions;       ///< Additional audio options
    TranscodeQuality quality;       ///< Quality preset
    bool preserveOriginalAudio;     ///< Keep original camera audio track

    /**
     * @brief Default constructor with ultra-fast test settings
     */
    TranscodeSettings()
        : videoCodec("-c:v libx264")
        , videoOptions("-preset ultrafast -crf 28 -s 640x360")  // Ultra fast, low res for testing
        , audioCodec("-c:a aac")
        , audioOptions("-b:a 64k -ar 22050")  // Low quality audio for speed
        , quality(TranscodeQuality::PROXY_LOW)
        , preserveOriginalAudio(true) {}
};

/**
 * @struct AudioMatch
 * @brief Result of audio-video matching and synchronization
 */
struct AudioMatch {
    std::filesystem::path highGainFile;     ///< Path to high-gain audio file
    std::filesystem::path lowGainFile;      ///< Path to low-gain audio file (optional)
    bool syncSuccess;                       ///< Whether sync detection succeeded
    double syncOffset;                      ///< Time offset in seconds (+ = lav starts after video)
    double confidenceScore;                 ///< Confidence in sync accuracy (0.0-1.0)
    SyncMethod method;                      ///< Method used for synchronization

    /**
     * @brief Default constructor
     */
    AudioMatch()
        : syncSuccess(false)
        , syncOffset(0.0)
        , confidenceScore(0.0)
        , method(SyncMethod::DURATION_MATCH) {}

    /**
     * @brief Check if this match has valid audio files
     * @return true if at least high-gain file exists
     */
    bool isValid() const {
        return !highGainFile.empty() && std::filesystem::exists(highGainFile);
    }

    /**
     * @brief Check if this match has both high and low gain files
     * @return true if both gain levels are available
     */
    bool hasBothGainLevels() const {
        return isValid() && !lowGainFile.empty() && std::filesystem::exists(lowGainFile);
    }
};

/**
 * @struct ProcessingResult
 * @brief Result of a single file transcoding operation
 */
struct ProcessingResult {
    std::filesystem::path inputVideo;       ///< Original video file
    std::filesystem::path outputFile;       ///< Generated output file
    AudioMatch audioMatch;                  ///< Audio matching details
    bool success;                          ///< Whether transcoding succeeded
    double processingTime;                 ///< Processing time in seconds
    std::string errorMessage;              ///< Error details if failed

    /**
     * @brief Default constructor
     */
    ProcessingResult()
        : success(false)
        , processingTime(0.0) {}
};

/**
 * @struct BatchProgress
 * @brief Progress tracking for batch operations
 */
struct BatchProgress {
    size_t totalFiles;                     ///< Total number of files to process
    size_t completedFiles;                 ///< Number of files completed
    size_t successfulFiles;                ///< Number of files processed successfully
    size_t failedFiles;                    ///< Number of files that failed
    double totalProcessingTime;            ///< Total time spent processing

    /**
     * @brief Default constructor
     */
    BatchProgress()
        : totalFiles(0)
        , completedFiles(0)
        , successfulFiles(0)
        , failedFiles(0)
        , totalProcessingTime(0.0) {}

    /**
     * @brief Calculate completion percentage
     * @return Percentage complete (0.0-100.0)
     */
    double getCompletionPercentage() const {
        return totalFiles > 0 ? (static_cast<double>(completedFiles) / totalFiles) * 100.0 : 0.0;
    }

    /**
     * @brief Calculate success rate
     * @return Success rate as percentage (0.0-100.0)
     */
    double getSuccessRate() const {
        return completedFiles > 0 ? (static_cast<double>(successfulFiles) / completedFiles) * 100.0 : 0.0;
    }
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

/// @brief Global transcoding settings (can be modified by command line args)
extern TranscodeSettings g_settings;

/// @brief Global duration tolerance for file matching
extern double g_durationTolerance;

/// @brief Enable verbose debug output
extern bool g_verboseOutput;

/// @brief Enable dry-run mode (no actual processing)
extern bool g_dryRun;

/// @brief Path to sync offset configuration file
extern std::string g_syncConfigFile;
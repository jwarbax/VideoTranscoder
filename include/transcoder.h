/**
 * @file transcoder.h
 * @brief Core video transcoding functionality with audio synchronization
 * @author Video Transcoder
 * @date 2025
 *
 * This header defines the main transcoder class that handles:
 * - Video file discovery and audio file matching
 * - Audio-video synchronization detection
 * - Batch transcoding operations
 * - Progress tracking and error handling
 */
#pragma once

#include "globals.h"
#include <chrono>
#include <functional>
#include <map>

// ============================================================================
// FORWARD DECLARATIONS AND STRUCTURES
// ============================================================================

/**
 * @struct OffsetCandidate
 * @brief Candidate offset with correlation score for sync detection
 */
struct OffsetCandidate {
    double offset;
    double score;

    // Default constructor
    OffsetCandidate() : offset(0.0), score(-100.0) {}

    // Parameterized constructor
    OffsetCandidate(double o, double s) : offset(o), score(s) {}
};

// ============================================================================
// CALLBACK TYPE DEFINITIONS
// ============================================================================

/**
 * @typedef ProgressCallback
 * @brief Callback function type for progress updates
 * @param progress Current batch progress information
 */
using ProgressCallback = std::function<void(const BatchProgress&)>;

/**
 * @typedef ErrorCallback
 * @brief Callback function type for error reporting
 * @param error Error message string
 * @param filename File that caused the error (optional)
 */
using ErrorCallback = std::function<void(const std::string& error, const std::string& filename)>;

// ============================================================================
// MAIN TRANSCODER CLASS
// ============================================================================

/**
 * @class VideoTranscoder
 * @brief Main class for video transcoding with audio synchronization
 *
 * This class provides the core functionality for:
 * - Discovering video and audio files in directories
 * - Matching audio files to video files based on duration and sync
 * - Detecting precise audio-video timing offsets
 * - Transcoding videos with synchronized external audio
 * - Batch processing with progress tracking
 */
class VideoTranscoder {
public:
    // ========================================================================
    // CONSTRUCTOR AND DESTRUCTOR
    // ========================================================================

    /**
     * @brief Construct a new Video Transcoder object
     */
    VideoTranscoder();

    /**
     * @brief Destroy the Video Transcoder object
     */
    ~VideoTranscoder() = default;

    // ========================================================================
    // FILE DISCOVERY AND FILTERING
    // ========================================================================

    /**
     * @brief Discover all video files in the specified directory
     * @param directory Directory to search for video files
     * @return Vector of video file paths
     */
    std::vector<std::filesystem::path> findVideoFiles(const std::filesystem::path& directory);

    /**
     * @brief Discover all audio files in the specified directory
     * @param directory Directory to search for audio files
     * @return Vector of audio file paths
     */
    std::vector<std::filesystem::path> findAudioFiles(const std::filesystem::path& directory);

    /**
     * @brief Filter files by supported video formats
     * @param files List of file paths to filter
     * @return Filtered list containing only supported video files
     */
    std::vector<std::filesystem::path> filterVideoFiles(const std::vector<std::filesystem::path>& files);

    /**
     * @brief Filter files by supported audio formats
     * @param files List of file paths to filter
     * @return Filtered list containing only supported audio files
     */
    std::vector<std::filesystem::path> filterAudioFiles(const std::vector<std::filesystem::path>& files);

    // ========================================================================
    // AUDIO-VIDEO MATCHING AND SYNCHRONIZATION
    // ========================================================================

    /**
     * @brief Find audio matches for all video files
     * @param videoFiles List of video files to match
     * @param audioFiles List of available audio files
     * @return Vector of audio matches (one per video file)
     */
    std::vector<AudioMatch> findAudioMatches(
        const std::vector<std::filesystem::path>& videoFiles,
        const std::vector<std::filesystem::path>& audioFiles
    );

    /**
     * @brief Find the best audio match for a single video file
     * @param videoFile Video file to match
     * @param audioFiles Available audio files for matching
     * @return Audio match result, or nullopt if no match found
     */
    std::optional<AudioMatch> findBestAudioMatch(
        const std::filesystem::path& videoFile,
        const std::vector<std::filesystem::path>& audioFiles
    );

    /**
     * @brief Detect precise timing offset between video and audio
     * @param videoFile Video file containing reference audio
     * @param audioFile External audio file to synchronize
     * @return Timing offset in seconds (+ = audio starts after video)
     */
    double detectSyncOffset(
        const std::filesystem::path& videoFile,
        const std::filesystem::path& audioFile
    );

    // ========================================================================
    // TRANSCODING OPERATIONS
    // ========================================================================

    /**
     * @brief Transcode a single video file with synchronized audio
     * @param videoFile Input video file
     * @param audioMatch Matched audio files and sync information
     * @param outputFile Desired output file path
     * @return Processing result with success/failure details
     */
    ProcessingResult transcodeVideo(
        const std::filesystem::path& videoFile,
        const AudioMatch& audioMatch,
        const std::filesystem::path& outputFile
    );

    /**
     * @brief Process all video files in a directory with batch operations
     * @param inputDirectory Directory containing video and audio files
     * @param outputDirectory Directory for transcoded outputs
     * @return Vector of processing results for all files
     */
    std::vector<ProcessingResult> processBatch(
        const std::filesystem::path& inputDirectory,
        const std::filesystem::path& outputDirectory
    );

    // ========================================================================
    // UTILITY AND HELPER FUNCTIONS
    // ========================================================================

    /**
     * @brief Get duration of a media file in seconds
     * @param filepath Path to the media file
     * @return Duration in seconds, or 0.0 if cannot be determined
     */
    static double getFileDuration(const std::filesystem::path& filepath);

    /**
     * @brief Check if a file is a high-gain audio file (based on naming)
     * @param filepath Audio file to check
     * @return true if this appears to be a high-gain file
     */
    static bool isHighGainFile(const std::filesystem::path& filepath);

    /**
     * @brief Find the low-gain counterpart of a high-gain audio file
     * @param highGainFile Path to high-gain audio file
     * @return Path to corresponding low-gain file, or empty if not found
     */
    static std::filesystem::path getLowGainCounterpart(const std::filesystem::path& highGainFile);

    /**
     * @brief Determine audio gain type from filename
     * @param filepath Audio file to analyze
     * @return Detected gain type
     */
    static AudioGainType determineGainType(const std::filesystem::path& filepath);

    /**
     * @brief Generate output filename based on input and settings
     * @param inputVideo Original video file
     * @param outputDirectory Target output directory
     * @return Generated output file path
     */
    std::filesystem::path generateOutputPath(
        const std::filesystem::path& inputVideo,
        const std::filesystem::path& outputDirectory
    ) const;

    // ========================================================================
    // CONFIGURATION AND CALLBACKS
    // ========================================================================

    /**
     * @brief Set progress callback for batch operations
     * @param callback Function to call with progress updates
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Set error callback for error reporting
     * @param callback Function to call when errors occur
     */
    void setErrorCallback(ErrorCallback callback);

    /**
     * @brief Load sync offsets from configuration file
     * @param configPath Path to sync configuration file
     * @return true if config loaded successfully
     */
    bool loadSyncConfig(const std::filesystem::path& configPath);

private:
    // ========================================================================
    // PRIVATE SYNC DETECTION METHODS
    // ========================================================================

    /**
     * @brief Perform automatic correlation-based sync detection
     * @param videoFile Video file with reference audio
     * @param audioFile External audio file
     * @return Detected offset in seconds, or 0.0 if detection failed
     */
    double performAutoSync(
        const std::filesystem::path& videoFile,
        const std::filesystem::path& audioFile
    );

    /**
     * @brief Search offset range using center-based sampling
     * @param minOffset Minimum offset to test
     * @param maxOffset Maximum offset to test
     * @param stepSize Step size for testing
     * @param videoFile Video file path
     * @param audioFile Audio file path
     * @param videoStart Start time for video sample
     * @param audioBaseStart Base start time for audio sample
     * @param sampleDuration Duration of samples to extract
     * @param tempDir Temporary directory
     * @return Vector of offset candidates with scores
     */
    std::vector<OffsetCandidate> searchOffsetRangeCenter(
        double minOffset,
        double maxOffset,
        double stepSize,
        const std::filesystem::path& videoFile,
        const std::filesystem::path& audioFile,
        double videoStart,
        double audioBaseStart,
        double sampleDuration,
        const std::string& tempDir
    );

    /**
     * @brief Test offset using center-based audio sampling
     * @param videoFile Video file path
     * @param audioFile Audio file path
     * @param offset Offset to test
     * @param videoStart Start time for video sample
     * @param audioBaseStart Base start time for audio sample
     * @param sampleDuration Duration of samples
     * @param tempDir Temporary directory
     * @return Correlation score
     */
    double testOffsetCenter(
        const std::filesystem::path& videoFile,
        const std::filesystem::path& audioFile,
        double offset,
        double videoStart,
        double audioBaseStart,
        double sampleDuration,
        const std::string& tempDir
    );

    /**
     * @brief Calculate cross-correlation between two audio files
     * @param audioFile1 First audio file
     * @param audioFile2 Second audio file
     * @return Correlation score (higher is better)
     */
    double calculateCrossCorrelation(
        const std::string& audioFile1,
        const std::string& audioFile2
    );

    /**
     * @brief Get RMS audio level of a file
     * @param audioFile Path to audio file
     * @return RMS level
     */
    double getAudioRMS(const std::string& audioFile);

    /**
     * @brief Test if files can sync with a specific offset
     * @param videoFile Video file with reference audio
     * @param audioFile External audio file
     * @param offset Offset to test (seconds)
     * @return true if sync test passes with this offset
     */
    bool testSyncOffset(
        const std::filesystem::path& videoFile,
        const std::filesystem::path& audioFile,
        double offset
    );

    /**
     * @brief Check if files match by duration within tolerance
     * @param videoFile Video file to check
     * @param audioFile Audio file to check
     * @return true if durations are within global tolerance
     */
    bool isDurationMatch(
        const std::filesystem::path& videoFile,
        const std::filesystem::path& audioFile
    ) const;

    // ========================================================================
    // PRIVATE UTILITY METHODS
    // ========================================================================

    /**
     * @brief Build FFmpeg command for transcoding
     * @param videoFile Input video file
     * @param audioMatch Audio files and sync information
     * @param outputFile Output file path
     * @return Complete FFmpeg command string
     */
    std::string buildFFmpegCommand(
        const std::filesystem::path& videoFile,
        const AudioMatch& audioMatch,
        const std::filesystem::path& outputFile
    ) const;

    /**
     * @brief Execute system command and measure execution time
     * @param command Command to execute
     * @return Execution time in seconds
     */
    double executeTimedCommand(const std::string& command);

    /**
     * @brief Report progress to callback if set
     * @param progress Current progress information
     */
    void reportProgress(const BatchProgress& progress);

    /**
     * @brief Report error to callback if set
     * @param error Error message
     * @param filename File that caused error (optional)
     */
    void reportError(const std::string& error, const std::string& filename = "");

    // ========================================================================
    // PRIVATE MEMBER VARIABLES
    // ========================================================================

    /// @brief Callback for progress updates
    ProgressCallback m_progressCallback;

    /// @brief Callback for error reporting
    ErrorCallback m_errorCallback;

    /// @brief Manual sync offsets loaded from config file
    std::map<std::pair<std::string, std::string>, double> m_syncOffsets;

    /// @brief Supported video file extensions
    static const std::vector<std::string> SUPPORTED_VIDEO_EXTENSIONS;

    /// @brief Supported audio file extensions
    static const std::vector<std::string> SUPPORTED_AUDIO_EXTENSIONS;
};
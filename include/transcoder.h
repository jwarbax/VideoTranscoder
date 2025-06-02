/**
 * @file transcoder.h
 * @brief Advanced video transcoder with hybrid audio synchronization
 */
#pragma once

#include "audio_sync.h"
#include <filesystem>
#include <vector>
#include <string>
#include <memory>
#include <map>

/**
 * @brief Synchronization statistics for reporting
 */
struct SyncStatistics {
    size_t totalFiles = 0;
    size_t successfulSyncs = 0;
    size_t highConfidenceSyncs = 0;
    size_t fallbackSyncs = 0;
    double avgConfidence = 0.0;
    double avgProcessingTime = 0.0;
    std::map<std::string, size_t> algorithmUsage;
    
    void addResult(const SyncResult& result);
    void printReport() const;
};

/**
 * @brief Advanced video transcoder with intelligent audio synchronization
 */
class VideoTranscoder {
public:
    VideoTranscoder();
    ~VideoTranscoder();
    
    /**
     * @brief Process all video files in input directory with advanced sync
     * @param inputDir Input directory path
     * @param outputDir Output directory path
     * @param syncQuality Quality mode for synchronization
     * @return True if all files processed successfully
     */
    bool processAll(const std::filesystem::path& inputDir, 
                   const std::filesystem::path& outputDir,
                   SyncQuality syncQuality = SyncQuality::STANDARD);

    /**
     * @brief Set verbose output mode
     * @param verbose Enable detailed logging
     */
    void setVerbose(bool verbose);
    
    /**
     * @brief Get synchronization statistics
     * @return Statistics from last processing run
     */
    const SyncStatistics& getSyncStatistics() const;
    
    /**
     * @brief Set confidence threshold for sync acceptance
     * @param threshold Minimum confidence (0.0-1.0) to accept sync result
     */
    void setConfidenceThreshold(float threshold);
    
    /**
     * @brief Enable/disable fallback processing for failed syncs
     * @param enableFallback Process files without sync if sync fails
     */
    void setFallbackProcessing(bool enableFallback);

private:
    /**
     * @brief Find all video files (.mp4, .MP4, .mov, .MOV)
     * @param directory Directory to search
     * @return Vector of video file paths
     */
    std::vector<std::filesystem::path> findVideoFiles(const std::filesystem::path& directory);
    
    /**
     * @brief Find all audio files (.wav, .WAV)
     * @param directory Directory to search
     * @return Vector of audio file paths
     */
    std::vector<std::filesystem::path> findAudioFiles(const std::filesystem::path& directory);
    
    /**
     * @brief Enhanced audio matching with multiple strategies
     * @param videoFile Video file path
     * @param audioFiles Available audio files
     * @return Tuple of high-gain, low-gain files, and match confidence
     */
    std::tuple<std::filesystem::path, std::filesystem::path, float> findAudioMatch(
        const std::filesystem::path& videoFile,
        const std::vector<std::filesystem::path>& audioFiles);
    
    /**
     * @brief Intelligent sync detection using hybrid algorithms
     * @param videoFile Video file path
     * @param audioFile Audio file path
     * @param quality Sync quality mode
     * @return Sync result with offset and confidence
     */
    SyncResult detectAdvancedSync(const std::filesystem::path& videoFile,
                                 const std::filesystem::path& audioFile,
                                 SyncQuality quality);
    
    /**
     * @brief Validate sync result and determine if acceptable
     * @param result Sync result to validate
     * @param videoFile Video file for context
     * @param audioFile Audio file for context
     * @return True if sync result is acceptable
     */
    bool validateSyncResult(const SyncResult& result,
                           const std::filesystem::path& videoFile,
                           const std::filesystem::path& audioFile);
    
    /**
     * @brief Transcode video with synchronized audio tracks
     * @param videoFile Input video file
     * @param highGainAudio High gain audio file
     * @param lowGainAudio Low gain audio file (can be empty)
     * @param syncResult Synchronization result with offset
     * @param outputFile Output file path
     * @return True if successful
     */
    bool transcodeWithSync(const std::filesystem::path& videoFile,
                          const std::filesystem::path& highGainAudio,
                          const std::filesystem::path& lowGainAudio,
                          const SyncResult& syncResult,
                          const std::filesystem::path& outputFile);
    
    /**
     * @brief Fallback transcoding without external audio sync
     * @param videoFile Input video file
     * @param outputFile Output file path
     * @return True if successful
     */
    bool transcodeFallback(const std::filesystem::path& videoFile,
                          const std::filesystem::path& outputFile);
    
    /**
     * @brief Get file duration in seconds using ffprobe
     * @param filepath Media file path
     * @return Duration in seconds
     */
    double getFileDuration(const std::filesystem::path& filepath);
    
    /**
     * @brief Check if two durations are compatible for sync
     * @param duration1 First duration in seconds
     * @param duration2 Second duration in seconds
     * @param tolerance Tolerance in seconds
     * @return True if durations are compatible
     */
    bool isDurationCompatible(double duration1, double duration2, double tolerance = 30.0);
    
    /**
     * @brief Generate detailed sync report for a file
     * @param videoFile Video file
     * @param audioFile Audio file
     * @param result Sync result
     */
    void logSyncDetails(const std::filesystem::path& videoFile,
                       const std::filesystem::path& audioFile,
                       const SyncResult& result);

    // Core components
    std::unique_ptr<HybridAudioSync> audioSync;
    SyncStatistics statistics;
    
    // Configuration
    bool verbose = true;
    float confidenceThreshold = 0.3f;
    bool fallbackProcessing = true;
    SyncQuality defaultQuality = SyncQuality::STANDARD;
};
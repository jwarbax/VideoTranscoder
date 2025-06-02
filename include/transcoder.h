/**
 * @file transcoder.h
 * @brief Minimal video transcoder with audio sync
 */
#pragma once

#include <filesystem>
#include <vector>
#include <string>

class VideoTranscoder {
public:
    VideoTranscoder();
    
    /**
     * @brief Process all video files in input directory
     * @param inputDir Input directory path
     * @param outputDir Output directory path
     * @return True if all files processed successfully
     */
    bool processAll(const std::filesystem::path& inputDir, 
                   const std::filesystem::path& outputDir);

private:
    /**
     * @brief Find all video files (.mp4, .MP4)
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
     * @brief Find matching audio files for a video
     * @param videoFile Video file path
     * @param audioFiles Available audio files
     * @return Pair of high-gain and low-gain files (empty if not found)
     */
    std::pair<std::filesystem::path, std::filesystem::path> findAudioMatch(
        const std::filesystem::path& videoFile,
        const std::vector<std::filesystem::path>& audioFiles);
    
    /**
     * @brief Detect sync offset between video and audio
     * @param videoFile Video file path
     * @param audioFile Audio file path
     * @return Offset in seconds (+ = audio starts after video)
     */
    double detectSyncOffset(const std::filesystem::path& videoFile,
                           const std::filesystem::path& audioFile);
    
    /**
     * @brief Transcode video with synced audio
     * @param videoFile Input video file
     * @param highGainAudio High gain audio file
     * @param lowGainAudio Low gain audio file (can be empty)
     * @param offset Audio offset in seconds
     * @param outputFile Output file path
     * @return True if successful
     */
    bool transcodeVideo(const std::filesystem::path& videoFile,
                       const std::filesystem::path& highGainAudio,
                       const std::filesystem::path& lowGainAudio,
                       double offset,
                       const std::filesystem::path& outputFile);
    
    /**
     * @brief Get file duration in seconds
     * @param filepath Media file path
     * @return Duration in seconds
     */
    double getFileDuration(const std::filesystem::path& filepath);
    
    bool m_verbose = true;  ///< Always verbose for now
};
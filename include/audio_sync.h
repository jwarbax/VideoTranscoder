/**
 * @file audio_sync.h
 * @brief Minimal audio synchronization using peak detection
 */
#pragma once

#include <filesystem>
#include <vector>

struct AudioPeak {
    double timestamp;    // Time in seconds
    double amplitude;    // Normalized amplitude 0-1
    
    AudioPeak(double t, double a) : timestamp(t), amplitude(a) {}
};

class AudioSync {
public:
    AudioSync();
    
    /**
     * @brief Find sync offset between video and audio using peak detection
     * @param videoFile Video file path
     * @param audioFile Audio file path
     * @return Offset in seconds (+ = audio starts after video)
     */
    double findOffset(const std::filesystem::path& videoFile,
                     const std::filesystem::path& audioFile);
    
    /**
     * @brief Enable/disable verbose output
     * @param verbose True for verbose output
     */
    void setVerbose(bool verbose);

private:
    /**
     * @brief Extract audio peaks from a media file
     * @param mediaFile Path to media file
     * @param startTime Start time for analysis
     * @param duration Duration to analyze
     * @return Vector of detected peaks
     */
    std::vector<AudioPeak> extractPeaks(const std::filesystem::path& mediaFile,
                                       double startTime = 0.0,
                                       double duration = 30.0);
    
    /**
     * @brief Find best offset by matching peak patterns
     * @param videoPeaks Peaks from video audio
     * @param audioPeaks Peaks from external audio
     * @return Best offset in seconds
     */
    double matchPeaks(const std::vector<AudioPeak>& videoPeaks,
                     const std::vector<AudioPeak>& audioPeaks);
    
    /**
     * @brief Calculate optimal analysis window
     * @param videoFile Video file
     * @param audioFile Audio file
     * @return Pair of start time and duration
     */
    std::pair<double, double> calculateAnalysisWindow(
        const std::filesystem::path& videoFile,
        const std::filesystem::path& audioFile);
    
    bool m_verbose = true;
};
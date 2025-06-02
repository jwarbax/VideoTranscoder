/**
 * @file audio_sync.h
 * @brief Minimal audio synchronization using peak interval patterns
 */
#pragma once

#include <filesystem>
#include <vector>

struct AudioPeak {
    double timestamp;    // Time in seconds
    double amplitude;    // Normalized amplitude 0-1
    
    AudioPeak(double t, double a) : timestamp(t), amplitude(a) {}
};

struct PeakPattern {
    std::vector<double> intervals;  // Time intervals between consecutive peaks
    double startTime;               // When this pattern starts
    
    PeakPattern() : startTime(0.0) {}
};

class AudioSync {
public:
    AudioSync();
    
    /**
     * @brief Find sync offset between video and audio using peak interval patterns
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
     * @brief Intelligently select the best 3 peaks from all detected peaks
     * @param allPeaks Vector of all detected peaks
     * @return Vector of the 3 most distinctive peaks
     */
    std::vector<AudioPeak> selectBest3Peaks(const std::vector<AudioPeak>& allPeaks);
    
    /**
     * @brief Convert peaks to interval patterns
     * @param peaks Vector of audio peaks
     * @return Peak pattern with intervals
     */
    PeakPattern createPattern(const std::vector<AudioPeak>& peaks);
    
    /**
     * @brief Compare two interval patterns directly
     * @param pattern1 First pattern
     * @param pattern2 Second pattern
     * @return Similarity score (0.0-1.0)
     */
    double comparePatterns(const PeakPattern& pattern1, const PeakPattern& pattern2);
    
    /**
     * @brief Find best offset by matching interval patterns
     * @param videoPattern Pattern from video audio
     * @param audioPattern Pattern from external audio
     * @return Best offset in seconds
     */
    double matchPatterns(const PeakPattern& videoPattern,
                        const PeakPattern& audioPattern);
    
    /**
     * @brief Calculate pattern similarity score
     * @param pattern1 First pattern
     * @param pattern2 Second pattern
     * @param offset Offset to test
     * @return Similarity score (0.0 = no match, 1.0 = perfect match)
     */
    double calculatePatternSimilarity(const PeakPattern& pattern1,
                                     const PeakPattern& pattern2,
                                     double offset);
    
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
/**
 * @file speech_sync.h
 * @brief Speech-optimized audio synchronization
 */
#pragma once

#include <filesystem>
#include <vector>

struct SpeechEvent {
    double timestamp;       // Time in seconds
    double energy;         // Speech energy level
    double spectralCentroid; // Frequency characteristic
    double duration;       // Duration of speech event
    
    SpeechEvent(double t, double e, double sc, double d) 
        : timestamp(t), energy(e), spectralCentroid(sc), duration(d) {}
};

struct SpeechPattern {
    std::vector<double> intervals;  // Time intervals between speech events
    std::vector<double> energyRatios; // Relative energy levels
    double startTime;               // When this pattern starts
    
    SpeechPattern() : startTime(0.0) {}
};

class SpeechSync {
public:
    SpeechSync();
    
    /**
     * @brief Find sync offset between video and audio using speech analysis
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
     * @brief Extract and normalize audio for speech analysis
     * @param mediaFile Path to media file
     * @param startTime Start time for analysis
     * @param duration Duration to analyze
     * @return Normalized audio samples
     */
    std::vector<float> extractNormalizedAudio(const std::filesystem::path& mediaFile,
                                             double startTime, double duration);
    
    /**
     * @brief Detect speech events in normalized audio
     * @param audioSamples Normalized audio samples
     * @param sampleRate Sample rate
     * @param startTime Original start time for timestamps
     * @return Vector of detected speech events
     */
    std::vector<SpeechEvent> detectSpeechEvents(const std::vector<float>& audioSamples,
                                               int sampleRate, double startTime);
    
    /**
     * @brief Apply aggressive normalization to audio
     * @param samples Raw audio samples
     * @return Normalized samples
     */
    std::vector<float> normalizeAudio(const std::vector<float>& samples);
    
    /**
     * @brief Calculate spectral centroid for audio window
     * @param samples Audio samples
     * @param startIndex Start index in samples
     * @param windowSize Window size in samples
     * @return Spectral centroid in Hz
     */
    double calculateSpectralCentroid(const std::vector<float>& samples, 
                                   int startIndex, int windowSize);
    
    /**
     * @brief Create speech pattern from events
     * @param events Vector of speech events
     * @return Speech pattern with intervals and characteristics
     */
    SpeechPattern createSpeechPattern(const std::vector<SpeechEvent>& events);
    
    /**
     * @brief Compare two speech patterns
     * @param pattern1 First speech pattern
     * @param pattern2 Second speech pattern
     * @return Similarity score (0.0-1.0)
     */
    double compareSpeechPatterns(const SpeechPattern& pattern1, 
                               const SpeechPattern& pattern2);
    
    /**
     * @brief Select best speech events from candidates
     * @param allEvents All detected events
     * @return Best 4-5 most distinctive events
     */
    std::vector<SpeechEvent> selectBestSpeechEvents(const std::vector<SpeechEvent>& allEvents);
    
    /**
     * @brief Calculate optimal analysis window for speech
     * @param videoFile Video file
     * @param audioFile Audio file
     * @return Pair of start time and duration
     */
    std::pair<double, double> calculateSpeechAnalysisWindow(
        const std::filesystem::path& videoFile,
        const std::filesystem::path& audioFile);
    
    bool m_verbose = true;
};
/**
 * @file audio_sync.h
 * @brief Advanced audio synchronization using multiple algorithms with confidence scoring
 */
#pragma once

#include <filesystem>
#include <vector>
#include <memory>
#include <complex>
#include <map>
#include <functional>

// Forward declarations for optimization libraries
namespace fftw {
    class FFTProcessor;
}

/**
 * @brief Audio feature extraction and analysis structures
 */
struct AudioFeatures {
    std::vector<float> mfcc;           // Mel-frequency cepstral coefficients
    std::vector<float> spectralCentroid; // Spectral centroid over time
    std::vector<float> energy;         // RMS energy envelope
    std::vector<float> zcr;           // Zero crossing rate
    std::vector<size_t> onsets;       // Onset detection points
    double sampleRate;
    size_t frameCount;
    
    AudioFeatures() : sampleRate(0.0), frameCount(0) {}
};

/**
 * @brief Synchronization result with confidence metrics
 */
struct SyncResult {
    double offset;                    // Offset in seconds (+ = audio2 starts after audio1)
    float confidence;                 // Confidence score 0.0-1.0
    std::string algorithm;            // Algorithm used for sync
    std::vector<float> confidenceProfile; // Per-frame confidence
    double computationTime;           // Time taken for computation
    
    SyncResult() : offset(0.0), confidence(0.0), computationTime(0.0) {}
};

/**
 * @brief Content type detection for algorithm selection
 */
enum class AudioContent {
    SPEECH,
    MUSIC,
    MIXED,
    SILENCE,
    NOISE,
    UNKNOWN
};

/**
 * @brief Processing quality modes
 */
enum class SyncQuality {
    REAL_TIME,     // <20ms latency, basic accuracy
    STANDARD,      // Good balance of speed and accuracy
    HIGH_QUALITY   // Maximum accuracy, longer processing
};

/**
 * @brief Base class for synchronization algorithms
 */
class SyncAlgorithm {
public:
    virtual ~SyncAlgorithm() = default;
    virtual SyncResult synchronize(const AudioFeatures& features1, 
                                 const AudioFeatures& features2) = 0;
    virtual std::string getName() const = 0;
    virtual float getExpectedAccuracy(AudioContent content) const = 0;
};

/**
 * @brief Cross-correlation based synchronization (optimized for speech)
 */
class CrossCorrelationSync : public SyncAlgorithm {
private:
    std::unique_ptr<fftw::FFTProcessor> fftProcessor;
    size_t windowSize;
    float adaptiveThreshold;
    
public:
    CrossCorrelationSync(size_t windowSize = 8192);
    ~CrossCorrelationSync();
    
    SyncResult synchronize(const AudioFeatures& features1, 
                         const AudioFeatures& features2) override;
    std::string getName() const override { return "CrossCorrelation"; }
    float getExpectedAccuracy(AudioContent content) const override;
    
private:
    std::vector<float> computeNormalizedCrossCorrelation(
        const std::vector<float>& signal1, 
        const std::vector<float>& signal2);
    void updateAdaptiveThreshold(const std::vector<float>& signal);
};

/**
 * @brief Dynamic Time Warping with MFCC features
 */
class DTWSync : public SyncAlgorithm {
private:
    size_t maxWarpingWindow;
    float slopeConstraint;
    bool useMultiScale;
    
public:
    DTWSync(size_t maxWarpingWindow = 1000, float slopeConstraint = 2.0f);
    
    SyncResult synchronize(const AudioFeatures& features1, 
                         const AudioFeatures& features2) override;
    std::string getName() const override { return "DTW"; }
    float getExpectedAccuracy(AudioContent content) const override;
    
private:
    std::vector<std::vector<float>> computeDTWMatrix(
        const std::vector<float>& features1,
        const std::vector<float>& features2);
    std::vector<std::pair<size_t, size_t>> traceback(
        const std::vector<std::vector<float>>& dtwMatrix);
    SyncResult multiScaleDTW(const AudioFeatures& features1, 
                           const AudioFeatures& features2);
};

/**
 * @brief Onset-based synchronization for percussive content
 */
class OnsetSync : public SyncAlgorithm {
private:
    float onsetThreshold;
    size_t minOnsetDistance;
    
public:
    OnsetSync(float threshold = 0.3f, size_t minDistance = 441); // 10ms at 44.1kHz
    
    SyncResult synchronize(const AudioFeatures& features1, 
                         const AudioFeatures& features2) override;
    std::string getName() const override { return "OnsetBased"; }
    float getExpectedAccuracy(AudioContent content) const override;
    
private:
    std::vector<size_t> detectOnsets(const std::vector<float>& spectralFlux);
    double alignOnsets(const std::vector<size_t>& onsets1, 
                      const std::vector<size_t>& onsets2);
};

/**
 * @brief Spectral correlation for music and tonal content
 */
class SpectralCorrelationSync : public SyncAlgorithm {
private:
    size_t fftSize;
    size_t hopSize;
    std::unique_ptr<fftw::FFTProcessor> fftProcessor;
    
public:
    SpectralCorrelationSync(size_t fftSize = 2048, size_t hopSize = 512);
    ~SpectralCorrelationSync();
    
    SyncResult synchronize(const AudioFeatures& features1, 
                         const AudioFeatures& features2) override;
    std::string getName() const override { return "SpectralCorrelation"; }
    float getExpectedAccuracy(AudioContent content) const override;
    
private:
    std::vector<std::vector<float>> computeSpectrograms(const std::vector<float>& audio);
    float computeSpectralSimilarity(const std::vector<std::vector<float>>& spec1,
                                   const std::vector<std::vector<float>>& spec2,
                                   int offset);
};

/**
 * @brief Rolling statistics for adaptive processing
 */
class RollingStatistics {
private:
    std::vector<float> window;
    size_t windowSize;
    size_t currentIndex;
    float sum;
    float sumSquared;
    bool filled;
    
public:
    explicit RollingStatistics(size_t windowSize = 100);
    
    void update(float value);
    float mean() const;
    float variance() const;
    float stdDev() const;
    void reset();
};

/**
 * @brief Main hybrid synchronization engine
 */
class HybridAudioSync {
public:
    HybridAudioSync();
    ~HybridAudioSync();
    
    /**
     * @brief Find optimal sync offset using hybrid approach
     */
    SyncResult findOptimalSync(const std::filesystem::path& audioFile1,
                              const std::filesystem::path& audioFile2,
                              SyncQuality quality = SyncQuality::STANDARD);
    
    /**
     * @brief Extract features from audio file
     */
    AudioFeatures extractFeatures(const std::filesystem::path& audioFile,
                                 double startTime = 0.0,
                                 double duration = 30.0);
    
    /**
     * @brief Detect audio content type for algorithm selection
     */
    AudioContent detectContentType(const AudioFeatures& features);
    
    /**
     * @brief Set processing quality mode
     */
    void setQualityMode(SyncQuality quality);
    
    /**
     * @brief Enable/disable verbose output
     */
    void setVerbose(bool verbose);
    
    /**
     * @brief Get performance statistics
     */
    std::map<std::string, double> getPerformanceStats() const;

private:
    std::vector<std::unique_ptr<SyncAlgorithm>> algorithms;
    std::map<AudioContent, std::vector<std::pair<size_t, float>>> algorithmWeights;
    SyncQuality currentQuality;
    bool verbose;
    mutable std::map<std::string, double> performanceStats;
    
    // Feature extraction components
    std::unique_ptr<fftw::FFTProcessor> mfccProcessor;
    
    /**
     * @brief Initialize algorithm weights for different content types
     */
    void initializeAlgorithmWeights();
    
    /**
     * @brief Combine results from multiple algorithms
     */
    SyncResult combineResults(const std::vector<SyncResult>& results,
                            const std::vector<float>& weights);
    
    /**
     * @brief Compute confidence score based on multiple factors
     */
    float computeConfidenceScore(const SyncResult& result,
                               const AudioFeatures& features1,
                               const AudioFeatures& features2);
    
    /**
     * @brief Extract MFCC features from audio samples
     */
    std::vector<float> extractMFCC(const std::vector<float>& audio,
                                  double sampleRate,
                                  size_t numCoeffs = 13);
    
    /**
     * @brief Extract spectral centroid
     */
    std::vector<float> extractSpectralCentroid(const std::vector<float>& audio,
                                              double sampleRate);
    
    /**
     * @brief Detect onsets using spectral flux
     */
    std::vector<size_t> detectOnsets(const std::vector<float>& audio,
                                    double sampleRate);
    
    /**
     * @brief Calculate analysis window for optimal performance
     */
    std::pair<double, double> calculateAnalysisWindow(
        const std::filesystem::path& audioFile1,
        const std::filesystem::path& audioFile2);
    
    /**
     * @brief Load audio samples from file
     */
    std::vector<float> loadAudioSamples(const std::filesystem::path& audioFile,
                                       double startTime,
                                       double duration,
                                       double& sampleRate);
};

/**
 * @brief Optimized FFT processor wrapper
 */
namespace fftw {
    class FFTProcessor {
    public:
        FFTProcessor(size_t size);
        ~FFTProcessor();
        
        void forward(const std::vector<float>& input,
                    std::vector<std::complex<float>>& output);
        void inverse(const std::vector<std::complex<float>>& input,
                    std::vector<float>& output);
        
        size_t getSize() const { return size; }
        
    private:
        size_t size;
        void* plan_forward;
        void* plan_inverse;
        float* input_buffer;
        std::complex<float>* output_buffer;
    };
}

/**
 * @brief Voice Activity Detection for speech processing
 */
class VoiceActivityDetector {
public:
    VoiceActivityDetector(double sampleRate = 44100.0);
    
    std::vector<bool> detectVoiceActivity(const std::vector<float>& audio);
    void setParameters(float energyThreshold, float zcrThreshold, float spectralEntropy);
    
private:
    double sampleRate;
    float energyThreshold;
    float zcrThreshold;
    float spectralEntropyThreshold;
    size_t frameSize;
    size_t hopSize;
    
    float computeSpectralEntropy(const std::vector<float>& frame);
    float computeZeroCrossingRate(const std::vector<float>& frame);
};
/**
 * @file audio_sync.cpp
 * @brief Advanced audio synchronization implementation
 */

#include "audio_sync.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <numeric>
#include <cstdlib>

// FFTW includes (conditional compilation)
#ifdef USE_FFTW
#include <fftw3.h>
#endif

namespace {
    // Mathematical constants
    constexpr float PI = 3.14159265359f;
    constexpr float TWO_PI = 2.0f * PI;
    
    // Audio processing constants
    constexpr size_t DEFAULT_SAMPLE_RATE = 44100;
    constexpr size_t MFCC_NUM_FILTERS = 26;
    constexpr size_t MFCC_FRAME_SIZE = 2048;
    constexpr size_t MFCC_HOP_SIZE = 512;
    
    // Synchronization parameters
    constexpr float MIN_CONFIDENCE_THRESHOLD = 0.3f;
    constexpr float HIGH_CONFIDENCE_THRESHOLD = 0.8f;
    constexpr size_t MAX_OFFSET_SAMPLES = 44100 * 30; // 30 seconds max offset
}

// ===========================
// FFT Processor Implementation
// ===========================

namespace fftw {
    FFTProcessor::FFTProcessor(size_t size) : size(size) {
#ifdef USE_FFTW
        input_buffer = fftwf_alloc_real(size);
        output_buffer = fftwf_alloc_complex(size/2 + 1);
        
        plan_forward = fftwf_plan_dft_r2c_1d(size, input_buffer, 
                                           reinterpret_cast<fftwf_complex*>(output_buffer),
                                           FFTW_ESTIMATE);
        plan_inverse = fftwf_plan_dft_c2r_1d(size, 
                                           reinterpret_cast<fftwf_complex*>(output_buffer),
                                           input_buffer, FFTW_ESTIMATE);
#else
        // Fallback implementation without FFTW
        input_buffer = new float[size];
        output_buffer = new std::complex<float>[size/2 + 1];
        plan_forward = nullptr;
        plan_inverse = nullptr;
#endif
    }
    
    FFTProcessor::~FFTProcessor() {
#ifdef USE_FFTW
        fftwf_destroy_plan(static_cast<fftwf_plan>(plan_forward));
        fftwf_destroy_plan(static_cast<fftwf_plan>(plan_inverse));
        fftwf_free(input_buffer);
        fftwf_free(output_buffer);
#else
        delete[] input_buffer;
        delete[] output_buffer;
#endif
    }
    
    void FFTProcessor::forward(const std::vector<float>& input,
                              std::vector<std::complex<float>>& output) {
        if (input.size() != size) {
            throw std::invalid_argument("Input size mismatch");
        }
        
        std::copy(input.begin(), input.end(), input_buffer);
        
#ifdef USE_FFTW
        fftwf_execute(static_cast<fftwf_plan>(plan_forward));
        output.resize(size/2 + 1);
        std::copy(output_buffer, output_buffer + size/2 + 1, output.begin());
#else
        // Simple DFT fallback (very slow, for compatibility only)
        output.resize(size/2 + 1);
        for (size_t k = 0; k < size/2 + 1; ++k) {
            std::complex<float> sum(0.0f, 0.0f);
            for (size_t n = 0; n < size; ++n) {
                float angle = -TWO_PI * k * n / size;
                sum += input_buffer[n] * std::complex<float>(std::cos(angle), std::sin(angle));
            }
            output[k] = sum;
        }
#endif
    }
    
    void FFTProcessor::inverse(const std::vector<std::complex<float>>& input,
                              std::vector<float>& output) {
        if (input.size() != size/2 + 1) {
            throw std::invalid_argument("Input size mismatch for inverse FFT");
        }
        
        std::copy(input.begin(), input.end(), output_buffer);
        
#ifdef USE_FFTW
        fftwf_execute(static_cast<fftwf_plan>(plan_inverse));
        output.resize(size);
        for (size_t i = 0; i < size; ++i) {
            output[i] = input_buffer[i] / size; // Normalize
        }
#else
        // Simple IDFT fallback
        output.resize(size);
        for (size_t n = 0; n < size; ++n) {
            std::complex<float> sum(0.0f, 0.0f);
            for (size_t k = 0; k < size/2 + 1; ++k) {
                float angle = TWO_PI * k * n / size;
                std::complex<float> mult = (k == 0 || k == size/2) ? 
                    input[k] : input[k] * 2.0f; // Account for negative frequencies
                sum += mult * std::complex<float>(std::cos(angle), std::sin(angle));
            }
            output[n] = sum.real() / size;
        }
#endif
    }
}

// ===========================
// Rolling Statistics Implementation
// ===========================

RollingStatistics::RollingStatistics(size_t windowSize) 
    : windowSize(windowSize), currentIndex(0), sum(0.0f), sumSquared(0.0f), filled(false) {
    window.resize(windowSize, 0.0f);
}

void RollingStatistics::update(float value) {
    if (filled) {
        // Remove old value
        float oldValue = window[currentIndex];
        sum -= oldValue;
        sumSquared -= oldValue * oldValue;
    }
    
    // Add new value
    window[currentIndex] = value;
    sum += value;
    sumSquared += value * value;
    
    currentIndex = (currentIndex + 1) % windowSize;
    if (currentIndex == 0) filled = true;
}

float RollingStatistics::mean() const {
    size_t count = filled ? windowSize : currentIndex;
    return count > 0 ? sum / count : 0.0f;
}

float RollingStatistics::variance() const {
    size_t count = filled ? windowSize : currentIndex;
    if (count < 2) return 0.0f;
    
    float meanVal = mean();
    return (sumSquared / count) - (meanVal * meanVal);
}

float RollingStatistics::stdDev() const {
    return std::sqrt(variance());
}

void RollingStatistics::reset() {
    currentIndex = 0;
    sum = 0.0f;
    sumSquared = 0.0f;
    filled = false;
    std::fill(window.begin(), window.end(), 0.0f);
}

// ===========================
// Cross-Correlation Sync Implementation
// ===========================

CrossCorrelationSync::CrossCorrelationSync(size_t windowSize) 
    : windowSize(windowSize), adaptiveThreshold(0.5f) {
    fftProcessor = std::make_unique<fftw::FFTProcessor>(windowSize);
}

CrossCorrelationSync::~CrossCorrelationSync() = default;

SyncResult CrossCorrelationSync::synchronize(const AudioFeatures& features1, 
                                           const AudioFeatures& features2) {
    auto start = std::chrono::high_resolution_clock::now();
    
    SyncResult result;
    result.algorithm = getName();
    
    // Use energy features for cross-correlation
    const auto& signal1 = features1.energy;
    const auto& signal2 = features2.energy;
    
    if (signal1.empty() || signal2.empty()) {
        result.confidence = 0.0f;
        return result;
    }
    
    // Compute normalized cross-correlation
    auto correlation = computeNormalizedCrossCorrelation(signal1, signal2);
    
    // Find peak
    auto maxIt = std::max_element(correlation.begin(), correlation.end());
    if (maxIt != correlation.end()) {
        size_t maxIndex = std::distance(correlation.begin(), maxIt);
        float maxValue = *maxIt;
        
        // Convert to time offset
        int offsetSamples = static_cast<int>(maxIndex) - static_cast<int>(signal1.size());
        result.offset = offsetSamples / features1.sampleRate;
        
        // Confidence based on peak height and sharpness
        result.confidence = std::min(1.0f, maxValue);
        
        // Improve confidence with parabolic interpolation
        if (maxIndex > 0 && maxIndex < correlation.size() - 1) {
            float y1 = correlation[maxIndex - 1];
            float y2 = correlation[maxIndex];
            float y3 = correlation[maxIndex + 1];
            
            float a = (y1 - 2*y2 + y3) / 2;
            if (std::abs(a) > 1e-6f) {
                float correction = -(y3 - y1) / (4 * a);
                result.offset += correction / features1.sampleRate;
                result.confidence *= 1.1f; // Bonus for sub-sample precision
            }
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.computationTime = std::chrono::duration<double>(end - start).count();
    
    return result;
}

std::vector<float> CrossCorrelationSync::computeNormalizedCrossCorrelation(
    const std::vector<float>& signal1, const std::vector<float>& signal2) {
    
    size_t len1 = signal1.size();
    size_t len2 = signal2.size();
    size_t resultSize = len1 + len2 - 1;
    
    // Pad to power of 2 for efficient FFT
    size_t fftSize = 1;
    while (fftSize < resultSize) fftSize <<= 1;
    
    std::vector<float> padded1(fftSize, 0.0f);
    std::vector<float> padded2(fftSize, 0.0f);
    
    std::copy(signal1.begin(), signal1.end(), padded1.begin());
    std::copy(signal2.begin(), signal2.end(), padded2.begin());
    
    // Compute FFTs
    std::vector<std::complex<float>> fft1, fft2;
    
    try {
        fftw::FFTProcessor processor(fftSize);
        processor.forward(padded1, fft1);
        processor.forward(padded2, fft2);
        
        // Compute cross-correlation in frequency domain
        std::vector<std::complex<float>> crossCorr(fft1.size());
        for (size_t i = 0; i < fft1.size(); ++i) {
            crossCorr[i] = fft1[i] * std::conj(fft2[i]);
        }
        
        // Inverse FFT
        std::vector<float> result;
        processor.inverse(crossCorr, result);
        
        // Extract relevant part and normalize
        std::vector<float> normalized(resultSize);
        for (size_t i = 0; i < resultSize; ++i) {
            normalized[i] = result[i];
        }
        
        // Normalize by local energy
        float norm1 = std::sqrt(std::inner_product(signal1.begin(), signal1.end(), 
                                                  signal1.begin(), 0.0f));
        float norm2 = std::sqrt(std::inner_product(signal2.begin(), signal2.end(), 
                                                  signal2.begin(), 0.0f));
        
        if (norm1 > 0 && norm2 > 0) {
            float normFactor = norm1 * norm2;
            for (auto& val : normalized) {
                val /= normFactor;
            }
        }
        
        return normalized;
        
    } catch (const std::exception& e) {
        // Fallback to simple time-domain correlation
        std::vector<float> result(resultSize, 0.0f);
        
        for (size_t lag = 0; lag < resultSize; ++lag) {
            float sum = 0.0f;
            size_t count = 0;
            
            for (size_t i = 0; i < len1; ++i) {
                int j = static_cast<int>(i) + static_cast<int>(lag) - static_cast<int>(len1);
                if (j >= 0 && j < static_cast<int>(len2)) {
                    sum += signal1[i] * signal2[j];
                    count++;
                }
            }
            
            result[lag] = count > 0 ? sum / count : 0.0f;
        }
        
        return result;
    }
}

float CrossCorrelationSync::getExpectedAccuracy(AudioContent content) const {
    switch (content) {
        case AudioContent::SPEECH: return 0.85f;
        case AudioContent::MUSIC: return 0.70f;
        case AudioContent::MIXED: return 0.75f;
        case AudioContent::SILENCE: return 0.10f;
        case AudioContent::NOISE: return 0.30f;
        default: return 0.60f;
    }
}

// ===========================
// DTW Sync Implementation
// ===========================

DTWSync::DTWSync(size_t maxWarpingWindow, float slopeConstraint) 
    : maxWarpingWindow(maxWarpingWindow), slopeConstraint(slopeConstraint), useMultiScale(true) {}

SyncResult DTWSync::synchronize(const AudioFeatures& features1, 
                               const AudioFeatures& features2) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (useMultiScale) {
        auto result = multiScaleDTW(features1, features2);
        auto end = std::chrono::high_resolution_clock::now();
        result.computationTime = std::chrono::duration<double>(end - start).count();
        return result;
    }
    
    SyncResult result;
    result.algorithm = getName();
    
    // Use MFCC features for DTW
    const auto& mfcc1 = features1.mfcc;
    const auto& mfcc2 = features2.mfcc;
    
    if (mfcc1.empty() || mfcc2.empty()) {
        result.confidence = 0.0f;
        return result;
    }
    
    // Compute DTW matrix
    auto dtwMatrix = computeDTWMatrix(mfcc1, mfcc2);
    
    // Traceback to find optimal path
    auto path = traceback(dtwMatrix);
    
    if (!path.empty()) {
        // Calculate average offset from path
        double totalOffset = 0.0;
        for (const auto& point : path) {
            totalOffset += static_cast<double>(point.second) - static_cast<double>(point.first);
        }
        
        double avgOffset = totalOffset / path.size();
        result.offset = avgOffset / features1.sampleRate * MFCC_HOP_SIZE;
        
        // Confidence based on path consistency and final cost
        float finalCost = dtwMatrix[mfcc1.size()-1][mfcc2.size()-1];
        result.confidence = std::max(0.0f, 1.0f - finalCost / 10.0f); // Normalize cost
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.computationTime = std::chrono::duration<double>(end - start).count();
    
    return result;
}

std::vector<std::vector<float>> DTWSync::computeDTWMatrix(
    const std::vector<float>& features1, const std::vector<float>& features2) {
    
    size_t len1 = features1.size();
    size_t len2 = features2.size();
    
    // Initialize DTW matrix
    std::vector<std::vector<float>> dtw(len1, std::vector<float>(len2, std::numeric_limits<float>::infinity()));
    
    // Initialize first cell
    dtw[0][0] = std::abs(features1[0] - features2[0]);
    
    // Fill first row and column
    for (size_t i = 1; i < len1; ++i) {
        dtw[i][0] = dtw[i-1][0] + std::abs(features1[i] - features2[0]);
    }
    for (size_t j = 1; j < len2; ++j) {
        dtw[0][j] = dtw[0][j-1] + std::abs(features1[0] - features2[j]);
    }
    
    // Fill the rest with slope constraints
    for (size_t i = 1; i < len1; ++i) {
        size_t jStart = std::max(1UL, static_cast<size_t>(i / slopeConstraint));
        size_t jEnd = std::min(len2, static_cast<size_t>(i * slopeConstraint) + 1);
        
        for (size_t j = jStart; j < jEnd; ++j) {
            float cost = std::abs(features1[i] - features2[j]);
            float minPrev = std::min({dtw[i-1][j], dtw[i][j-1], dtw[i-1][j-1]});
            dtw[i][j] = cost + minPrev;
        }
    }
    
    return dtw;
}

std::vector<std::pair<size_t, size_t>> DTWSync::traceback(
    const std::vector<std::vector<float>>& dtwMatrix) {
    
    std::vector<std::pair<size_t, size_t>> path;
    
    size_t i = dtwMatrix.size() - 1;
    size_t j = dtwMatrix[0].size() - 1;
    
    while (i > 0 || j > 0) {
        path.emplace_back(i, j);
        
        if (i == 0) {
            j--;
        } else if (j == 0) {
            i--;
        } else {
            // Find minimum predecessor
            float diagonal = dtwMatrix[i-1][j-1];
            float vertical = dtwMatrix[i-1][j];
            float horizontal = dtwMatrix[i][j-1];
            
            if (diagonal <= vertical && diagonal <= horizontal) {
                i--; j--;
            } else if (vertical <= horizontal) {
                i--;
            } else {
                j--;
            }
        }
    }
    
    path.emplace_back(0, 0);
    std::reverse(path.begin(), path.end());
    
    return path;
}

SyncResult DTWSync::multiScaleDTW(const AudioFeatures& features1, 
                                 const AudioFeatures& features2) {
    SyncResult result;
    result.algorithm = getName() + "_MultiScale";
    
    // Multi-scale processing: 8x, 4x, 2x, 1x downsampling
    std::vector<int> scales = {8, 4, 2, 1};
    double currentOffset = 0.0;
    
    for (int scale : scales) {
        // Downsample MFCC features
        std::vector<float> downsampled1, downsampled2;
        
        for (size_t i = 0; i < features1.mfcc.size(); i += scale) {
            downsampled1.push_back(features1.mfcc[i]);
        }
        for (size_t i = 0; i < features2.mfcc.size(); i += scale) {
            downsampled2.push_back(features2.mfcc[i]);
        }
        
        if (downsampled1.empty() || downsampled2.empty()) continue;
        
        // Compute DTW at this scale
        auto dtwMatrix = computeDTWMatrix(downsampled1, downsampled2);
        auto path = traceback(dtwMatrix);
        
        if (!path.empty()) {
            double scaleOffset = 0.0;
            for (const auto& point : path) {
                scaleOffset += static_cast<double>(point.second) - static_cast<double>(point.first);
            }
            scaleOffset = (scaleOffset / path.size()) * scale;
            
            // Refine previous estimate
            currentOffset = (currentOffset + scaleOffset) / 2.0;
            
            // Update confidence based on path consistency
            float pathVariance = 0.0f;
            double avgOffset = scaleOffset / path.size();
            for (const auto& point : path) {
                double pointOffset = static_cast<double>(point.second) - static_cast<double>(point.first);
                pathVariance += (pointOffset - avgOffset) * (pointOffset - avgOffset);
            }
            pathVariance /= path.size();
            
            result.confidence = std::max(result.confidence, 
                                       std::max(0.0f, 1.0f - pathVariance / 100.0f));
        }
    }
    
    result.offset = currentOffset / features1.sampleRate * MFCC_HOP_SIZE;
    return result;
}

float DTWSync::getExpectedAccuracy(AudioContent content) const {
    switch (content) {
        case AudioContent::SPEECH: return 0.90f;
        case AudioContent::MUSIC: return 0.85f;
        case AudioContent::MIXED: return 0.80f;
        case AudioContent::SILENCE: return 0.20f;
        case AudioContent::NOISE: return 0.40f;
        default: return 0.70f;
    }
}

// ===========================
// Onset Sync Implementation
// ===========================

OnsetSync::OnsetSync(float threshold, size_t minDistance) 
    : onsetThreshold(threshold), minOnsetDistance(minDistance) {}

SyncResult OnsetSync::synchronize(const AudioFeatures& features1, 
                                 const AudioFeatures& features2) {
    auto start = std::chrono::high_resolution_clock::now();
    
    SyncResult result;
    result.algorithm = getName();
    
    const auto& onsets1 = features1.onsets;
    const auto& onsets2 = features2.onsets;
    
    if (onsets1.size() < 3 || onsets2.size() < 3) {
        result.confidence = 0.0f;
        return result;
    }
    
    result.offset = alignOnsets(onsets1, onsets2);
    
    // Confidence based on number of onsets and their distribution
    size_t minOnsets = std::min(onsets1.size(), onsets2.size());
    result.confidence = std::min(1.0f, minOnsets / 10.0f);
    
    auto end = std::chrono::high_resolution_clock::now();
    result.computationTime = std::chrono::duration<double>(end - start).count();
    
    return result;
}

double OnsetSync::alignOnsets(const std::vector<size_t>& onsets1, 
                             const std::vector<size_t>& onsets2) {
    if (onsets1.empty() || onsets2.empty()) return 0.0;
    
    // Try different alignments and find the best match
    double bestOffset = 0.0;
    int bestScore = 0;
    
    for (size_t i = 0; i < std::min(onsets1.size(), size_t(5)); ++i) {
        for (size_t j = 0; j < std::min(onsets2.size(), size_t(5)); ++j) {
            double offset = static_cast<double>(onsets2[j]) - static_cast<double>(onsets1[i]);
            
            // Count matching onsets within tolerance
            int score = 0;
            const double tolerance = 1000.0; // samples (about 23ms at 44.1kHz)
            
            for (size_t k = 0; k < onsets1.size(); ++k) {
                double expectedPos = onsets1[k] + offset;
                
                for (size_t l = 0; l < onsets2.size(); ++l) {
                    if (std::abs(onsets2[l] - expectedPos) < tolerance) {
                        score++;
                        break;
                    }
                }
            }
            
            if (score > bestScore) {
                bestScore = score;
                bestOffset = offset;
            }
        }
    }
    
    return bestOffset;
}

float OnsetSync::getExpectedAccuracy(AudioContent content) const {
    switch (content) {
        case AudioContent::SPEECH: return 0.60f;
        case AudioContent::MUSIC: return 0.95f;
        case AudioContent::MIXED: return 0.75f;
        case AudioContent::SILENCE: return 0.05f;
        case AudioContent::NOISE: return 0.15f;
        default: return 0.50f;
    }
}

// ===========================
// Spectral Correlation Sync Implementation
// ===========================

SpectralCorrelationSync::SpectralCorrelationSync(size_t fftSize, size_t hopSize) 
    : fftSize(fftSize), hopSize(hopSize) {
    fftProcessor = std::make_unique<fftw::FFTProcessor>(fftSize);
}

SpectralCorrelationSync::~SpectralCorrelationSync() = default;

SyncResult SpectralCorrelationSync::synchronize(const AudioFeatures& features1, 
                                               const AudioFeatures& features2) {
    auto start = std::chrono::high_resolution_clock::now();
    
    SyncResult result;
    result.algorithm = getName();
    
    // This would need the raw audio data, which we don't have in AudioFeatures
    // For now, use spectral centroid as a proxy
    const auto& centroid1 = features1.spectralCentroid;
    const auto& centroid2 = features2.spectralCentroid;
    
    if (centroid1.empty() || centroid2.empty()) {
        result.confidence = 0.0f;
        return result;
    }
    
    // Find best correlation of spectral centroids
    size_t maxLag = std::min(centroid1.size(), centroid2.size()) / 2;
    float bestCorr = -1.0f;
    int bestLag = 0;

    for (int lag = -static_cast<int>(maxLag); lag <= static_cast<int>(maxLag); ++lag) {
        float correlation = 0.0f;
        int count = 0;

        for (size_t i = 0; i < centroid1.size(); ++i) {
            int j = static_cast<int>(i) + lag;
            if (j >= 0 && j < static_cast<int>(centroid2.size())) {
                correlation += centroid1[i] * centroid2[j];
                count++;
            }
        }

        if (count > 0) {
            correlation /= count;
            if (correlation > bestCorr) {
                bestCorr = correlation;
                bestLag = lag;
            }
        }
    }

    result.offset = static_cast<double>(bestLag) * static_cast<double>(hopSize) / features1.sampleRate;
    result.confidence = std::max(0.0f, bestCorr);

    auto end = std::chrono::high_resolution_clock::now();
    result.computationTime = std::chrono::duration<double>(end - start).count();
    
    return result;
}

float SpectralCorrelationSync::getExpectedAccuracy(AudioContent content) const {
    switch (content) {
        case AudioContent::SPEECH: return 0.70f;
        case AudioContent::MUSIC: return 0.90f;
        case AudioContent::MIXED: return 0.80f;
        case AudioContent::SILENCE: return 0.10f;
        case AudioContent::NOISE: return 0.25f;
        default: return 0.65f;
    }
}

// ===========================
// Hybrid Audio Sync Implementation
// ===========================

HybridAudioSync::HybridAudioSync() : currentQuality(SyncQuality::STANDARD), verbose(false) {
    // Initialize algorithms
    algorithms.push_back(std::make_unique<CrossCorrelationSync>());
    algorithms.push_back(std::make_unique<DTWSync>());
    algorithms.push_back(std::make_unique<OnsetSync>());
    algorithms.push_back(std::make_unique<SpectralCorrelationSync>());
    
    initializeAlgorithmWeights();
    
    // Initialize MFCC processor
    mfccProcessor = std::make_unique<fftw::FFTProcessor>(MFCC_FRAME_SIZE);
}

HybridAudioSync::~HybridAudioSync() = default;

void HybridAudioSync::initializeAlgorithmWeights() {
    // Weights: [CrossCorr, DTW, Onset, Spectral] for each content type
    algorithmWeights[AudioContent::SPEECH] = {{0, 0.4f}, {1, 0.4f}, {2, 0.1f}, {3, 0.1f}};
    algorithmWeights[AudioContent::MUSIC] = {{0, 0.2f}, {1, 0.3f}, {2, 0.3f}, {3, 0.2f}};
    algorithmWeights[AudioContent::MIXED] = {{0, 0.3f}, {1, 0.3f}, {2, 0.2f}, {3, 0.2f}};
    algorithmWeights[AudioContent::SILENCE] = {{0, 0.7f}, {1, 0.2f}, {2, 0.05f}, {3, 0.05f}};
    algorithmWeights[AudioContent::NOISE] = {{0, 0.5f}, {1, 0.3f}, {2, 0.1f}, {3, 0.1f}};
    algorithmWeights[AudioContent::UNKNOWN] = {{0, 0.35f}, {1, 0.35f}, {2, 0.15f}, {3, 0.15f}};
}

SyncResult HybridAudioSync::findOptimalSync(const std::filesystem::path& audioFile1,
                                           const std::filesystem::path& audioFile2,
                                           SyncQuality quality) {
    setQualityMode(quality);
    
    if (verbose) {
        std::cout << "\n🎵 Advanced Hybrid Audio Synchronization" << std::endl;
        std::cout << "===========================================" << std::endl;
        std::cout << "Audio 1: " << audioFile1.filename().string() << std::endl;
        std::cout << "Audio 2: " << audioFile2.filename().string() << std::endl;
        std::cout << "Quality: ";
        switch (quality) {
            case SyncQuality::REAL_TIME: std::cout << "Real-time"; break;
            case SyncQuality::STANDARD: std::cout << "Standard"; break;
            case SyncQuality::HIGH_QUALITY: std::cout << "High Quality"; break;
        }
        std::cout << std::endl;
    }
    
    // Extract features from both audio files
    auto features1 = extractFeatures(audioFile1);
    auto features2 = extractFeatures(audioFile2);
    
    if (features1.frameCount == 0 || features2.frameCount == 0) {
        SyncResult result;
        result.confidence = 0.0f;
        if (verbose) {
            std::cout << "❌ Failed to extract audio features" << std::endl;
        }
        return result;
    }
    
    // Detect content type
    AudioContent contentType = detectContentType(features1);
    if (verbose) {
        std::cout << "🎯 Content type: ";
        switch (contentType) {
            case AudioContent::SPEECH: std::cout << "Speech"; break;
            case AudioContent::MUSIC: std::cout << "Music"; break;
            case AudioContent::MIXED: std::cout << "Mixed"; break;
            case AudioContent::SILENCE: std::cout << "Silence"; break;
            case AudioContent::NOISE: std::cout << "Noise"; break;
            default: std::cout << "Unknown"; break;
        }
        std::cout << std::endl;
    }
    
    // Run synchronization algorithms
    std::vector<SyncResult> results;
    std::vector<float> weights;
    
    for (size_t i = 0; i < algorithms.size(); ++i) {
        auto result = algorithms[i]->synchronize(features1, features2);
        
        if (verbose) {
            std::cout << "📊 " << result.algorithm << ": offset=" << result.offset 
                      << "s, confidence=" << result.confidence 
                      << ", time=" << result.computationTime << "s" << std::endl;
        }
        
        results.push_back(result);
        
        // Get weight for this algorithm and content type
        float weight = 0.0f;
        for (const auto& pair : algorithmWeights[contentType]) {
            if (pair.first == i) {
                weight = pair.second;
                break;
            }
        }
        weights.push_back(weight);
    }
    
    // Combine results
    auto finalResult = combineResults(results, weights);
    finalResult.offset = -finalResult.offset;
    finalResult.confidence = computeConfidenceScore(finalResult, features1, features2);
    
    if (verbose) {
        std::cout << "🎯 Final result: offset=" << finalResult.offset 
                  << "s, confidence=" << finalResult.confidence << std::endl;
        
        if (finalResult.confidence < MIN_CONFIDENCE_THRESHOLD) {
            std::cout << "⚠️  Low confidence result - consider manual verification" << std::endl;
        } else if (finalResult.confidence > HIGH_CONFIDENCE_THRESHOLD) {
            std::cout << "✅ High confidence result" << std::endl;
        }
    }
    
    return finalResult;
}

AudioFeatures HybridAudioSync::extractFeatures(const std::filesystem::path& audioFile,
                                               double startTime, double duration) {
    AudioFeatures features;
    
    double sampleRate;
    auto audioSamples = loadAudioSamples(audioFile, startTime, duration, sampleRate);
    
    if (audioSamples.empty()) {
        return features;
    }
    
    features.sampleRate = sampleRate;
    features.frameCount = audioSamples.size() / MFCC_HOP_SIZE;
    
    // Extract MFCC features
    features.mfcc = extractMFCC(audioSamples, sampleRate);
    
    // Extract spectral centroid
    features.spectralCentroid = extractSpectralCentroid(audioSamples, sampleRate);
    
    // Extract energy envelope
    features.energy.reserve(features.frameCount);
    for (size_t i = 0; i < audioSamples.size(); i += MFCC_HOP_SIZE) {
        float energy = 0.0f;
        size_t endIdx = std::min(i + MFCC_HOP_SIZE, audioSamples.size());
        for (size_t j = i; j < endIdx; ++j) {
            energy += audioSamples[j] * audioSamples[j];
        }
        features.energy.push_back(std::sqrt(energy / (endIdx - i)));
    }
    
    // Extract zero crossing rate
    features.zcr.reserve(features.frameCount);
    for (size_t i = 0; i < audioSamples.size(); i += MFCC_HOP_SIZE) {
        int crossings = 0;
        size_t endIdx = std::min(i + MFCC_HOP_SIZE, audioSamples.size());
        for (size_t j = i + 1; j < endIdx; ++j) {
            if ((audioSamples[j] >= 0) != (audioSamples[j-1] >= 0)) {
                crossings++;
            }
        }
        features.zcr.push_back(static_cast<float>(crossings) / (endIdx - i));
    }
    
    // Detect onsets
    features.onsets = detectOnsets(audioSamples, sampleRate);
    
    return features;
}

std::vector<float> HybridAudioSync::loadAudioSamples(const std::filesystem::path& audioFile,
                                                    double startTime, double duration,
                                                    double& sampleRate) {
    // Create temporary raw audio file
    std::string tempAudio = "/tmp/audio_hybrid_" + std::to_string(std::time(nullptr)) + 
                           "_" + std::to_string(rand() % 10000) + ".raw";
    
    std::ostringstream extractCmd;
    extractCmd << "ffmpeg -hide_banner -loglevel error ";
    extractCmd << "-ss " << startTime << " ";
    extractCmd << "-i \"" << audioFile.string() << "\" ";
    extractCmd << "-t " << duration << " ";
    extractCmd << "-vn -f f32le -acodec pcm_f32le -ar 44100 -ac 1 ";
    extractCmd << "\"" << tempAudio << "\"";
    
    int result = std::system(extractCmd.str().c_str());
    if (result != 0) {
        std::filesystem::remove(tempAudio);
        return {};
    }
    
    // Read the audio data
    std::ifstream file(tempAudio, std::ios::binary);
    if (!file) {
        std::filesystem::remove(tempAudio);
        return {};
    }
    
    std::vector<float> samples;
    float sample;
    while (file.read(reinterpret_cast<char*>(&sample), sizeof(float))) {
        samples.push_back(sample);
    }
    
    file.close();
    std::filesystem::remove(tempAudio);
    
    sampleRate = 44100.0; // We forced this in ffmpeg
    return samples;
}

AudioContent HybridAudioSync::detectContentType(const AudioFeatures& features) {
    if (features.energy.empty()) return AudioContent::UNKNOWN;
    
    // Calculate energy statistics
    float avgEnergy = std::accumulate(features.energy.begin(), features.energy.end(), 0.0f) / features.energy.size();
    float maxEnergy = *std::max_element(features.energy.begin(), features.energy.end());
    
    // Calculate ZCR statistics
    float avgZCR = features.zcr.empty() ? 0.0f : 
        std::accumulate(features.zcr.begin(), features.zcr.end(), 0.0f) / features.zcr.size();
    
    // Silence detection
    if (avgEnergy < 0.01f && maxEnergy < 0.05f) {
        return AudioContent::SILENCE;
    }
    
    // Speech detection (moderate ZCR, moderate energy dynamics)
    if (avgZCR > 0.1f && avgZCR < 0.3f && features.onsets.size() < 20) {
        return AudioContent::SPEECH;
    }
    
    // Music detection (lower ZCR, more onsets)
    if (avgZCR < 0.15f && features.onsets.size() > 15) {
        return AudioContent::MUSIC;
    }
    
    // High ZCR suggests noise
    if (avgZCR > 0.4f) {
        return AudioContent::NOISE;
    }
    
    return AudioContent::MIXED;
}

SyncResult HybridAudioSync::combineResults(const std::vector<SyncResult>& results,
                                          const std::vector<float>& weights) {
    SyncResult combined;
    combined.algorithm = "Hybrid";
    
    if (results.empty() || weights.empty()) {
        return combined;
    }
    
    float totalWeight = 0.0f;
    float weightedOffset = 0.0f;
    float weightedConfidence = 0.0f;
    double totalTime = 0.0;
    
    for (size_t i = 0; i < std::min(results.size(), weights.size()); ++i) {
        float adjustedWeight = weights[i] * results[i].confidence;
        
        weightedOffset += results[i].offset * adjustedWeight;
        weightedConfidence += results[i].confidence * adjustedWeight;
        totalWeight += adjustedWeight;
        totalTime += results[i].computationTime;
    }
    
    if (totalWeight > 0.0f) {
        combined.offset = weightedOffset / totalWeight;
        combined.confidence = weightedConfidence / totalWeight;
    }
    
    combined.computationTime = totalTime;
    
    return combined;
}

float HybridAudioSync::computeConfidenceScore(const SyncResult& result,
                                             const AudioFeatures& features1,
                                             const AudioFeatures& features2) {
    float confidence = result.confidence;
    
    // Boost confidence based on feature quality
    if (!features1.mfcc.empty() && !features2.mfcc.empty()) {
        confidence *= 1.1f;
    }
    
    if (features1.onsets.size() > 5 && features2.onsets.size() > 5) {
        confidence *= 1.05f;
    }
    
    // Penalize very large offsets
    if (std::abs(result.offset) > 10.0) {
        confidence *= 0.8f;
    }
    
    return std::min(1.0f, confidence);
}

void HybridAudioSync::setQualityMode(SyncQuality quality) {
    currentQuality = quality;
    
    // Adjust algorithm parameters based on quality
    switch (quality) {
        case SyncQuality::REAL_TIME:
            // Reduce processing for speed
            break;
        case SyncQuality::STANDARD:
            // Balanced settings
            break;
        case SyncQuality::HIGH_QUALITY:
            // Maximum accuracy settings
            break;
    }
}

void HybridAudioSync::setVerbose(bool verbose) {
    this->verbose = verbose;
}

std::map<std::string, double> HybridAudioSync::getPerformanceStats() const {
    return performanceStats;
}

// Placeholder implementations for feature extraction methods
std::vector<float> HybridAudioSync::extractMFCC(const std::vector<float>& audio,
                                                double sampleRate, size_t numCoeffs) {
    // Simplified MFCC extraction - in practice, use a dedicated library
    std::vector<float> mfcc;
    
    size_t numFrames = (audio.size() - MFCC_FRAME_SIZE) / MFCC_HOP_SIZE + 1;
    
    for (size_t frame = 0; frame < numFrames; ++frame) {
        size_t startIdx = frame * MFCC_HOP_SIZE;
        
        // Extract frame
        std::vector<float> frameData(audio.begin() + startIdx, 
                                   audio.begin() + startIdx + MFCC_FRAME_SIZE);
        
        // Simple spectral centroid as MFCC approximation
        float centroid = 0.0f;
        float totalMagnitude = 0.0f;
        
        for (size_t i = 0; i < frameData.size(); ++i) {
            float magnitude = std::abs(frameData[i]);
            centroid += i * magnitude;
            totalMagnitude += magnitude;
        }
        
        if (totalMagnitude > 0) {
            centroid /= totalMagnitude;
        }
        
        mfcc.push_back(centroid);
    }
    
    return mfcc;
}

std::vector<float> HybridAudioSync::extractSpectralCentroid(const std::vector<float>& audio,
                                                           double sampleRate) {
    std::vector<float> centroids;
    
    size_t numFrames = (audio.size() - MFCC_FRAME_SIZE) / MFCC_HOP_SIZE + 1;
    
    for (size_t frame = 0; frame < numFrames; ++frame) {
        size_t startIdx = frame * MFCC_HOP_SIZE;
        
        float centroid = 0.0f;
        float totalEnergy = 0.0f;
        
        for (size_t i = 0; i < MFCC_FRAME_SIZE && startIdx + i < audio.size(); ++i) {
            float energy = audio[startIdx + i] * audio[startIdx + i];
            centroid += i * energy;
            totalEnergy += energy;
        }
        
        if (totalEnergy > 0) {
            centroid = (centroid / totalEnergy) * (sampleRate / 2.0f) / MFCC_FRAME_SIZE;
        }
        
        centroids.push_back(centroid);
    }
    
    return centroids;
}

std::vector<size_t> HybridAudioSync::detectOnsets(const std::vector<float>& audio,
                                                  double sampleRate) {
    std::vector<size_t> onsets;
    
    // Simple onset detection using energy increases
    std::vector<float> energy;
    size_t windowSize = static_cast<size_t>(sampleRate * 0.02); // 20ms windows
    
    for (size_t i = 0; i < audio.size(); i += windowSize) {
        float frameEnergy = 0.0f;
        size_t endIdx = std::min(i + windowSize, audio.size());
        
        for (size_t j = i; j < endIdx; ++j) {
            frameEnergy += audio[j] * audio[j];
        }
        
        energy.push_back(frameEnergy / (endIdx - i));
    }
    
    // Find energy peaks
    float threshold = 0.1f;
    for (size_t i = 1; i < energy.size() - 1; ++i) {
        if (energy[i] > threshold && 
            energy[i] > energy[i-1] && 
            energy[i] > energy[i+1]) {
            onsets.push_back(i * windowSize);
        }
    }
    
    return onsets;
}

std::pair<double, double> HybridAudioSync::calculateAnalysisWindow(
    const std::filesystem::path& audioFile1, const std::filesystem::path& audioFile2) {
    
    // Use middle 30 seconds by default, similar to original implementation
    return {10.0, 30.0};
}
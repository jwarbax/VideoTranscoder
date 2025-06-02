/**
 * @file speech_sync.cpp
 * @brief Speech-optimized sync implementation
 */

#include "speech_sync.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <sstream>

SpeechSync::SpeechSync() {
}

void SpeechSync::setVerbose(bool verbose) {
    m_verbose = verbose;
}

double SpeechSync::findOffset(const std::filesystem::path& videoFile,
                            const std::filesystem::path& audioFile) {
    
    if (m_verbose) {
        std::cout << "\n🎤 Speech-optimized sync detection..." << std::endl;
        std::cout << "  Video: " << videoFile.filename().string() << std::endl;
        std::cout << "  Audio: " << audioFile.filename().string() << std::endl;
    }
    
    // Calculate optimal analysis window for speech
    auto [videoStart, duration] = calculateSpeechAnalysisWindow(videoFile, audioFile);
    
    if (m_verbose) {
        std::cout << "  Speech analysis window: " << videoStart << "s + " << duration << "s" << std::endl;
    }
    
    // Extract and analyze video speech (reference)
    auto videoAudio = extractNormalizedAudio(videoFile, videoStart, duration);
    auto videoEvents = detectSpeechEvents(videoAudio, 16000, videoStart);
    
    if (videoEvents.size() < 3) {
        if (m_verbose) {
            std::cout << "  ⚠️  Not enough speech events in video, returning zero offset" << std::endl;
        }
        return 0.0;
    }
    
    // Create speech pattern from video
    SpeechPattern videoPattern = createSpeechPattern(videoEvents);
    
    if (m_verbose) {
        std::cout << "  🎤 Video speech events: " << videoEvents.size() << std::endl;
        std::cout << "  📊 Video pattern intervals: ";
        for (size_t i = 0; i < std::min(videoPattern.intervals.size(), size_t(4)); ++i) {
            std::cout << videoPattern.intervals[i] << "s ";
        }
        std::cout << std::endl;
    }
    
    // Test different audio extraction windows
    double bestOffset = 0.0;
    double bestScore = 0.0;
    double bestAudioStartTime = 0.0;
    
    if (m_verbose) {
        std::cout << "  🔍 Testing speech pattern matches..." << std::endl;
    }
    
    // Test audio extraction at different positions
    for (double audioStart = videoStart - 10.0; audioStart <= videoStart + 10.0; audioStart += 0.5) {
        
        if (audioStart < 0) continue;
        
        // Extract and analyze audio speech at this position
        auto audioAudio = extractNormalizedAudio(audioFile, audioStart, duration);
        auto audioEvents = detectSpeechEvents(audioAudio, 16000, audioStart);
        
        if (audioEvents.size() < 3) continue;
        
        // Create speech pattern
        SpeechPattern audioPattern = createSpeechPattern(audioEvents);
        
        // Compare speech patterns
        double score = compareSpeechPatterns(videoPattern, audioPattern);
        
        if (score > bestScore) {
            bestScore = score;
            bestAudioStartTime = audioStart;
            bestOffset = videoStart - audioStart;  // How much to shift audio backwards
        }
        
        if (m_verbose && score > 0.15) {  // Lower threshold for showing results
            std::cout << "    Audio " << audioStart << "s: speech score " << score 
                      << " (offset: " << (videoStart - audioStart) << "s)" << std::endl;
        }
    }
    
    if (m_verbose) {
        std::cout << "  🎯 Best speech match score: " << bestScore << std::endl;
        std::cout << "  🎯 Best audio position: " << bestAudioStartTime << "s" << std::endl;
        std::cout << "  🎯 Video reference: " << videoStart << "s" << std::endl;
        std::cout << "  🎯 Calculated offset: " << bestOffset << "s" << std::endl;
    }
    
    if (bestScore < 0.2) {  // Lower threshold for speech (was 0.3)
        if (m_verbose) {
            std::cout << "  ⚠️  No good speech pattern match, returning zero offset" << std::endl;
        }
        return 0.0;
    }
    
    return bestOffset;
}

std::vector<float> SpeechSync::extractNormalizedAudio(const std::filesystem::path& mediaFile,
                                                     double startTime, double duration) {
    
    if (m_verbose) {
        std::cout << "    Extracting normalized audio from " << mediaFile.filename().string() << std::endl;
    }
    
    // Extract raw audio at higher sample rate for speech analysis
    std::string tempAudio = "/tmp/speech_raw_" + std::to_string(std::time(nullptr)) + 
                           "_" + std::to_string(rand() % 10000) + ".raw";
    
    std::ostringstream extractCmd;
    extractCmd << "ffmpeg -hide_banner -loglevel error ";
    extractCmd << "-ss " << startTime << " ";
    extractCmd << "-i \"" << mediaFile.string() << "\" ";
    extractCmd << "-t " << duration << " ";
    extractCmd << "-vn -f f32le -acodec pcm_f32le -ar 16000 -ac 1 ";
    extractCmd << "\"" << tempAudio << "\"";
    
    int result = std::system(extractCmd.str().c_str());
    if (result != 0) {
        if (m_verbose) {
            std::cout << "    ❌ Failed to extract audio" << std::endl;
        }
        std::filesystem::remove(tempAudio);
        return {};
    }
    
    // Read audio samples
    FILE* audioFile = fopen(tempAudio.c_str(), "rb");
    if (!audioFile) {
        std::filesystem::remove(tempAudio);
        return {};
    }
    
    std::vector<float> samples;
    float sample;
    while (fread(&sample, sizeof(float), 1, audioFile) == 1) {
        samples.push_back(sample);
    }
    fclose(audioFile);
    std::filesystem::remove(tempAudio);
    
    if (m_verbose) {
        std::cout << "    ✅ Extracted " << samples.size() << " samples (" 
                  << (samples.size() / 16000.0) << "s)" << std::endl;
    }
    
    // Apply aggressive normalization
    return normalizeAudio(samples);
}

std::vector<float> SpeechSync::normalizeAudio(const std::vector<float>& samples) {
    
    if (samples.empty()) return samples;
    
    std::vector<float> normalized = samples;
    
    // Step 1: Remove DC offset
    float mean = 0.0f;
    for (float sample : normalized) {
        mean += sample;
    }
    mean /= normalized.size();
    
    for (float& sample : normalized) {
        sample -= mean;
    }
    
    // Step 2: Automatic Gain Control (AGC)
    // Find RMS level
    float rms = 0.0f;
    for (float sample : normalized) {
        rms += sample * sample;
    }
    rms = std::sqrt(rms / normalized.size());
    
    // Normalize to target RMS level
    float targetRMS = 0.1f;  // Target level
    if (rms > 0.001f) {  // Avoid division by zero
        float gain = targetRMS / rms;
        for (float& sample : normalized) {
            sample *= gain;
        }
    }
    
    // Step 3: Soft limiting to prevent clipping
    for (float& sample : normalized) {
        if (sample > 0.95f) sample = 0.95f;
        if (sample < -0.95f) sample = -0.95f;
    }
    
    if (m_verbose) {
        std::cout << "    🔧 Applied normalization: RMS " << rms << " → " << targetRMS << std::endl;
    }
    
    return normalized;
}

std::vector<SpeechEvent> SpeechSync::detectSpeechEvents(const std::vector<float>& audioSamples,
                                                       int sampleRate, double startTime) {
    
    std::vector<SpeechEvent> events;
    
    if (audioSamples.empty()) return events;
    
    // Speech detection parameters
    int windowSize = sampleRate * 0.05;  // 50ms windows for speech analysis
    int hopSize = windowSize / 4;        // 75% overlap
    float speechThreshold = 0.02f;       // Energy threshold for speech
    
    std::vector<SpeechEvent> candidates;
    
    // Analyze in overlapping windows
    for (int i = 0; i + windowSize < static_cast<int>(audioSamples.size()); i += hopSize) {
        
        // Calculate energy in this window
        float energy = 0.0f;
        for (int j = i; j < i + windowSize; ++j) {
            energy += audioSamples[j] * audioSamples[j];
        }
        energy = std::sqrt(energy / windowSize);  // RMS energy
        
        // Check if this might be speech
        if (energy > speechThreshold) {
            
            // Calculate spectral centroid for this window
            double spectralCentroid = calculateSpectralCentroid(audioSamples, i, windowSize);
            
            double timestamp = startTime + (i / static_cast<double>(sampleRate));
            
            // Look for speech onset (significant energy increase)
            bool isOnset = true;
            if (!candidates.empty()) {
                double timeDiff = timestamp - candidates.back().timestamp;
                if (timeDiff < 0.1) {  // Less than 100ms apart
                    // Check if this is significantly louder
                    if (energy < candidates.back().energy * 1.5) {
                        isOnset = false;
                    }
                }
            }
            
            if (isOnset) {
                candidates.emplace_back(timestamp, energy, spectralCentroid, 0.05);
            }
        }
    }
    
    if (m_verbose) {
        std::cout << "    Found " << candidates.size() << " speech candidates" << std::endl;
    }
    
    // Select best speech events
    events = selectBestSpeechEvents(candidates);
    
    if (m_verbose) {
        std::cout << "    ✅ Selected " << events.size() << " speech events:" << std::endl;
        for (size_t i = 0; i < events.size(); ++i) {
            std::cout << "      Event #" << (i+1) << " at " << events[i].timestamp 
                      << "s (energy: " << events[i].energy 
                      << ", freq: " << events[i].spectralCentroid << "Hz)" << std::endl;
        }
    }
    
    return events;
}

double SpeechSync::calculateSpectralCentroid(const std::vector<float>& samples, 
                                           int startIndex, int windowSize) {
    
    // Simple spectral centroid calculation
    // In a real implementation, you'd use FFT
    
    // For now, estimate based on zero-crossing rate and energy distribution
    int zeroCrossings = 0;
    float highFreqEnergy = 0.0f;
    float totalEnergy = 0.0f;
    
    for (int i = startIndex; i < startIndex + windowSize - 1; ++i) {
        if (i >= static_cast<int>(samples.size()) - 1) break;
        
        // Count zero crossings (rough frequency estimate)
        if ((samples[i] >= 0 && samples[i+1] < 0) || (samples[i] < 0 && samples[i+1] >= 0)) {
            zeroCrossings++;
        }
        
        // High frequency energy (difference between adjacent samples)
        float diff = std::abs(samples[i+1] - samples[i]);
        highFreqEnergy += diff * diff;
        totalEnergy += samples[i] * samples[i];
    }
    
    // Estimate spectral centroid
    double zcRate = static_cast<double>(zeroCrossings) / (windowSize - 1);
    double estimatedFreq = zcRate * 8000.0;  // Rough estimate
    
    // Adjust based on high frequency content
    if (totalEnergy > 0) {
        double hfRatio = highFreqEnergy / totalEnergy;
        estimatedFreq *= (1.0 + hfRatio);
    }
    
    return std::max(100.0, std::min(4000.0, estimatedFreq));  // Clamp to reasonable range
}

SpeechPattern SpeechSync::createSpeechPattern(const std::vector<SpeechEvent>& events) {
    
    SpeechPattern pattern;
    
    if (events.empty()) return pattern;
    
    pattern.startTime = events[0].timestamp;
    
    // Store frequency characteristics
    for (const auto& event : events) {
        pattern.frequencies.push_back(event.spectralCentroid);
    }
    
    // Calculate intervals and energy ratios
    for (size_t i = 1; i < events.size(); ++i) {
        double interval = events[i].timestamp - events[i-1].timestamp;
        pattern.intervals.push_back(interval);
        
        // Energy ratio (relative loudness)
        double ratio = events[i].energy / events[i-1].energy;
        pattern.energyRatios.push_back(ratio);
    }
    
    return pattern;
}

double SpeechSync::compareSpeechPatterns(const SpeechPattern& pattern1, 
                                       const SpeechPattern& pattern2) {
    
    if (pattern1.intervals.empty() || pattern2.intervals.empty()) {
        return 0.0;
    }
    
    double intervalScore = 0.0;
    double energyScore = 0.0;
    double frequencyScore = 0.0;
    int comparisons = 0;
    
    // Compare intervals (timing patterns) - less important for speech
    size_t maxIntervals = std::min(pattern1.intervals.size(), pattern2.intervals.size());
    
    for (size_t i = 0; i < maxIntervals; ++i) {
        double diff = std::abs(pattern1.intervals[i] - pattern2.intervals[i]);
        double tolerance = 2.0;  // Much more realistic tolerance for speech intervals
        
        if (diff < tolerance) {
            intervalScore += (tolerance - diff) / tolerance;
        }
        
        // Compare energy ratios - MUCH more important
        if (i < pattern1.energyRatios.size() && i < pattern2.energyRatios.size()) {
            double ratio1 = pattern1.energyRatios[i];
            double ratio2 = pattern2.energyRatios[i];
            
            // Avoid log of zero or negative numbers
            if (ratio1 > 0.1 && ratio2 > 0.1) {
                double energyDiff = std::abs(std::log(ratio1) - std::log(ratio2));
                
                if (energyDiff < 1.2) {  // Lenient energy ratio difference
                    energyScore += (1.2 - energyDiff) / 1.2;
                }
            }
        }
        
        comparisons++;
    }
    
    // Compare frequency characteristics - very important for voice matching
    size_t maxFreqs = std::min(pattern1.frequencies.size(), pattern2.frequencies.size());
    int freqComparisons = 0;
    
    for (size_t i = 0; i < maxFreqs; ++i) {
        double freq1 = pattern1.frequencies[i];
        double freq2 = pattern2.frequencies[i];
        
        // Compare frequency similarity
        double freqDiff = std::abs(freq1 - freq2);
        double freqTolerance = 200.0;  // 200Hz tolerance for voice characteristics
        
        if (freqDiff < freqTolerance) {
            frequencyScore += (freqTolerance - freqDiff) / freqTolerance;
        }
        
        freqComparisons++;
    }
    
    if (comparisons == 0) return 0.0;
    
    // NEW WEIGHTING: Energy (40%) + Frequency (40%) + Timing (20%)
    double totalScore = 0.0;
    
    if (comparisons > 0) {
        totalScore += (energyScore / comparisons) * 0.4;
        totalScore += (intervalScore / comparisons) * 0.2;
    }
    
    if (freqComparisons > 0) {
        totalScore += (frequencyScore / freqComparisons) * 0.4;
    }
    
    if (m_verbose && totalScore > 0.1) {
        std::cout << "      Pattern comparison: interval=" << (intervalScore/std::max(1,comparisons)) 
                  << ", energy=" << (energyScore/std::max(1,comparisons))
                  << ", frequency=" << (frequencyScore/std::max(1,freqComparisons))
                  << ", total=" << totalScore << std::endl;
    }
    
    return std::min(1.0, totalScore);
}

std::vector<SpeechEvent> SpeechSync::selectBestSpeechEvents(const std::vector<SpeechEvent>& allEvents) {
    
    if (allEvents.size() <= 4) {
        return allEvents;
    }
    
    // Sort by energy (strongest speech events first)
    std::vector<SpeechEvent> candidates = allEvents;
    std::sort(candidates.begin(), candidates.end(),
              [](const SpeechEvent& a, const SpeechEvent& b) {
                  return a.energy > b.energy;
              });
    
    std::vector<SpeechEvent> selected;
    
    // Select up to 4 events with minimum separation
    for (const auto& candidate : candidates) {
        bool farEnough = true;
        
        for (const auto& selected_event : selected) {
            if (std::abs(candidate.timestamp - selected_event.timestamp) < 0.3) {  // 300ms minimum
                farEnough = false;
                break;
            }
        }
        
        if (farEnough) {
            selected.push_back(candidate);
            if (selected.size() >= 4) break;  // Limit to 4 events
        }
    }
    
    // Sort by timestamp for pattern creation
    std::sort(selected.begin(), selected.end(),
              [](const SpeechEvent& a, const SpeechEvent& b) {
                  return a.timestamp < b.timestamp;
              });
    
    return selected;
}

std::pair<double, double> SpeechSync::calculateSpeechAnalysisWindow(
    const std::filesystem::path& videoFile,
    const std::filesystem::path& audioFile) {
    
    // Get file durations
    auto getDuration = [](const std::filesystem::path& file) -> double {
        std::string cmd = "ffprobe -v quiet -show_entries format=duration "
                         "-of csv=p=0 \"" + file.string() + "\" 2>/dev/null";
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return 0.0;
        
        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
        
        try {
            return std::stod(result);
        } catch (...) {
            return 0.0;
        }
    };
    
    double videoDuration = getDuration(videoFile);
    double audioDuration = getDuration(audioFile);
    
    if (videoDuration <= 0 || audioDuration <= 0) {
        return {5.0, 15.0};  // Default fallback for speech
    }
    
    // For speech analysis, use shorter windows from early in the recording
    // People usually start talking early, and early speech is cleaner
    double analysisDuration = std::min(15.0, videoDuration * 0.2);  // 20% of file, max 15s
    analysisDuration = std::max(8.0, analysisDuration);   // Minimum 8s for speech
    
    double startTime = std::min(5.0, videoDuration * 0.1);  // Start early but not at very beginning
    startTime = std::max(0.0, startTime);
    
    return {startTime, analysisDuration};
}
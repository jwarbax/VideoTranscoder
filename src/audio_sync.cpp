/**
 * @file audio_sync.cpp
 * @brief Audio sync implementation using peak interval patterns
 */

#include "audio_sync.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <sstream>

AudioSync::AudioSync() {
}

void AudioSync::setVerbose(bool verbose) {
    m_verbose = verbose;
}

double AudioSync::findOffset(const std::filesystem::path& videoFile,
                            const std::filesystem::path& audioFile) {
    
    if (m_verbose) {
        std::cout << "\n🔍 Detecting sync using peak interval patterns..." << std::endl;
        std::cout << "  Video: " << videoFile.filename().string() << std::endl;
        std::cout << "  Audio: " << audioFile.filename().string() << std::endl;
    }
    
    // Calculate optimal analysis window for video (reference)
    auto [videoStart, duration] = calculateAnalysisWindow(videoFile, audioFile);
    
    if (m_verbose) {
        std::cout << "  Video reference window: " << videoStart << "s + " << duration << "s" << std::endl;
    }
    
    // Extract peaks from video (our reference)
    auto videoPeaks = extractPeaks(videoFile, videoStart, duration);
    
    if (videoPeaks.size() < 3) {
        if (m_verbose) {
            std::cout << "  ⚠️  Not enough video peaks, returning zero offset" << std::endl;
        }
        return 0.0;
    }
    
    // Create video pattern
    PeakPattern videoPattern = createPattern(videoPeaks);
    
    if (m_verbose) {
        std::cout << "  Video peaks: " << videoPeaks.size() << std::endl;
        std::cout << "  Video pattern: " << videoPattern.intervals.size() << " intervals" << std::endl;
        std::cout << "  Video intervals: ";
        for (size_t i = 0; i < std::min(videoPattern.intervals.size(), size_t(5)); ++i) {
            std::cout << videoPattern.intervals[i] << "s ";
        }
        std::cout << std::endl;
    }
    
    // Now test different audio extraction windows to find where the pattern matches
    double bestOffset = 0.0;
    double bestScore = 0.0;
    double bestAudioStartTime = 0.0;  // Track which audio start time gave best match
    
    if (m_verbose) {
        std::cout << "  🔍 Testing audio windows for pattern match..." << std::endl;
    }
    
    // Test audio extraction starting from different times
    for (double audioStart = videoStart - 15.0; audioStart <= videoStart + 15.0; audioStart += 1.0) {
        
        if (audioStart < 0) continue; // Can't start before file beginning
        
        // Extract peaks from audio at this position
        auto audioPeaks = extractPeaks(audioFile, audioStart, duration);
        
        if (audioPeaks.size() < 3) continue;
        
        // Create audio pattern
        PeakPattern audioPattern = createPattern(audioPeaks);
        
        // Compare patterns
        double score = comparePatterns(videoPattern, audioPattern);
        
        if (score > bestScore) {
            bestScore = score;
            bestAudioStartTime = audioStart;  // Remember the audio extraction start time
            
            // CORRECT OFFSET CALCULATION:
            // If we extracted video from videoStart and audio from audioStart,
            // and the patterns match, then:
            // offset = audioStart - videoStart
            bestOffset = audioStart - videoStart;
        }
        
        if (m_verbose && score > 0.5) {
            std::cout << "    Audio start " << audioStart << "s: pattern score " << score 
                      << " (offset would be " << (audioStart - videoStart) << "s)" << std::endl;
        }
    }
    
    if (m_verbose) {
        std::cout << "  🎯 Best pattern match score: " << bestScore << std::endl;
        std::cout << "  🎯 Best audio start time: " << bestAudioStartTime << "s" << std::endl;
        std::cout << "  🎯 Video start time: " << videoStart << "s" << std::endl;
        std::cout << "  🎯 Calculated offset: " << bestOffset << "s" << std::endl;
        std::cout << "  📝 This means: audio needs to be shifted " << bestOffset << "s relative to video" << std::endl;
    }
    
    if (bestScore < 0.3) {
        if (m_verbose) {
            std::cout << "  ⚠️  No good pattern match found, returning zero offset" << std::endl;
        }
        return 0.0;
    }
    
    return bestOffset;
}

// New helper method to compare two patterns directly
double AudioSync::comparePatterns(const PeakPattern& pattern1, const PeakPattern& pattern2) {
    
    if (pattern1.intervals.empty() || pattern2.intervals.empty()) {
        return 0.0;
    }
    
    // Find best alignment position
    double bestScore = 0.0;
    
    // Try aligning pattern2 at different positions within pattern1
    int maxShift = std::min(3, static_cast<int>(std::max(pattern1.intervals.size(), pattern2.intervals.size())));
    
    for (int shift = -maxShift; shift <= maxShift; ++shift) {
        
        double score = 0.0;
        int comparisons = 0;
        
        for (size_t i = 0; i < pattern1.intervals.size(); ++i) {
            int j = static_cast<int>(i) + shift;
            
            if (j >= 0 && j < static_cast<int>(pattern2.intervals.size())) {
                double diff = std::abs(pattern1.intervals[i] - pattern2.intervals[j]);
                double tolerance = 0.3; // 300ms tolerance
                
                if (diff < tolerance) {
                    score += (tolerance - diff) / tolerance;
                }
                comparisons++;
            }
        }
        
        if (comparisons > 0) {
            score /= comparisons; // Normalize
            bestScore = std::max(bestScore, score);
        }
    }
    
    return bestScore;
}

std::vector<AudioPeak> AudioSync::extractPeaks(const std::filesystem::path& mediaFile,
                                              double startTime,
                                              double duration) {
    
    std::vector<AudioPeak> peaks;
    
    if (m_verbose) {
        std::cout << "    Extracting peaks from " << mediaFile.filename().string() << std::endl;
    }
    
    // Extract raw audio data
    std::string tempAudio = "/tmp/audio_raw_" + std::to_string(std::time(nullptr)) + 
                           "_" + std::to_string(rand() % 10000) + ".raw";
    
    std::ostringstream extractCmd;
    extractCmd << "ffmpeg -hide_banner -loglevel error ";
    extractCmd << "-ss " << startTime << " ";
    extractCmd << "-i \"" << mediaFile.string() << "\" ";
    extractCmd << "-t " << duration << " ";
    extractCmd << "-vn -f f32le -acodec pcm_f32le -ar 8000 -ac 1 ";
    extractCmd << "\"" << tempAudio << "\"";
    
    if (m_verbose) {
        std::cout << "    FFmpeg extract: " << extractCmd.str() << std::endl;
    }
    
    int result = std::system(extractCmd.str().c_str());
    if (result != 0) {
        if (m_verbose) {
            std::cout << "    ❌ Failed to extract audio" << std::endl;
        }
        std::filesystem::remove(tempAudio);
        return peaks;
    }
    
    // Read the audio data
    FILE* audioFile = fopen(tempAudio.c_str(), "rb");
    if (!audioFile) {
        if (m_verbose) {
            std::cout << "    ❌ Failed to open extracted audio file" << std::endl;
        }
        std::filesystem::remove(tempAudio);
        return peaks;
    }
    
    // Read all samples
    std::vector<float> samples;
    float sample;
    while (fread(&sample, sizeof(float), 1, audioFile) == 1) {
        samples.push_back(std::abs(sample)); // Take absolute value
    }
    fclose(audioFile);
    std::filesystem::remove(tempAudio);
    
    if (samples.empty()) {
        if (m_verbose) {
            std::cout << "    ❌ No audio samples extracted" << std::endl;
        }
        return peaks;
    }
    
    if (m_verbose) {
        std::cout << "    ✅ Extracted " << samples.size() << " samples (" 
                  << (samples.size() / 8000.0) << "s)" << std::endl;
    }
    
    // Simple peak detection using envelope and thresholding
    int windowSize = 800;  // 100ms at 8kHz
    float threshold = 0.01f; // Lower threshold to catch more events
    
    std::vector<float> envelope;
    
    // Calculate envelope using moving average
    for (size_t i = 0; i < samples.size(); ++i) {
        float sum = 0.0f;
        int count = 0;
        
        int start = std::max(0, static_cast<int>(i) - windowSize/2);
        int end = std::min(static_cast<int>(samples.size()), static_cast<int>(i) + windowSize/2);
        
        for (int j = start; j < end; ++j) {
            sum += samples[j];
            count++;
        }
        
        envelope.push_back(count > 0 ? sum / count : 0.0f);
    }
    
    // Find ALL peaks first
    std::vector<AudioPeak> allPeaks;
    
    if (!envelope.empty()) {
        float maxEnv = *std::max_element(envelope.begin(), envelope.end());
        float adaptiveThreshold = maxEnv * 0.2f; // Lower threshold to catch more peaks
        threshold = std::max(threshold, adaptiveThreshold);
        
        if (m_verbose) {
            std::cout << "    Max envelope: " << maxEnv << ", threshold: " << threshold << std::endl;
        }
        
        int minDistance = 2000; // 250ms minimum between peaks (was 500ms)
        int lastPeakIndex = -minDistance;
        
        for (size_t i = 1; i < envelope.size() - 1; ++i) {
            // Check if this is a local maximum above threshold
            if (envelope[i] > threshold && 
                envelope[i] > envelope[i-1] && 
                envelope[i] > envelope[i+1] &&
                static_cast<int>(i) - lastPeakIndex >= minDistance) {
                
                double timestamp = startTime + (i / 8000.0);
                double amplitude = envelope[i] / maxEnv; // Normalize
                
                allPeaks.emplace_back(timestamp, amplitude);
                lastPeakIndex = static_cast<int>(i);
                
                // Don't limit here - collect all peaks first
                if (allPeaks.size() >= 50) {
                    break; // Reasonable upper limit
                }
            }
        }
    }
    
    if (m_verbose) {
        std::cout << "    Found " << allPeaks.size() << " total peaks" << std::endl;
    }
    
    // Now intelligently select the best 3 peaks
    peaks = selectBest3Peaks(allPeaks);
    
    if (m_verbose) {
        std::cout << "    ✅ Selected " << peaks.size() << " best peaks:" << std::endl;
        for (size_t i = 0; i < peaks.size(); ++i) {
            std::cout << "      Peak #" << (i+1) << " at " << peaks[i].timestamp 
                      << "s (amplitude: " << peaks[i].amplitude << ")" << std::endl;
        }
    }
    
    return peaks;
}

// New method to intelligently select the best 3 peaks
std::vector<AudioPeak> AudioSync::selectBest3Peaks(const std::vector<AudioPeak>& allPeaks) {
    
    if (allPeaks.size() <= 3) {
        return allPeaks; // Already 3 or fewer
    }
    
    std::vector<AudioPeak> candidates = allPeaks;
    
    // Sort by amplitude (strongest first)
    std::sort(candidates.begin(), candidates.end(),
              [](const AudioPeak& a, const AudioPeak& b) {
                  return a.amplitude > b.amplitude;
              });
    
    std::vector<AudioPeak> selected;
    
    // Strategy 1: Take the strongest peak first
    selected.push_back(candidates[0]);
    
    if (m_verbose) {
        std::cout << "      Selected strongest peak: " << candidates[0].timestamp 
                  << "s (amp: " << candidates[0].amplitude << ")" << std::endl;
    }
    
    // Strategy 2: Find the next strongest peak that's at least 1 second away
    for (size_t i = 1; i < candidates.size() && selected.size() < 3; ++i) {
        bool farEnough = true;
        
        for (const auto& selectedPeak : selected) {
            if (std::abs(candidates[i].timestamp - selectedPeak.timestamp) < 1.0) {
                farEnough = false;
                break;
            }
        }
        
        if (farEnough) {
            selected.push_back(candidates[i]);
            if (m_verbose) {
                std::cout << "      Selected distant peak: " << candidates[i].timestamp 
                          << "s (amp: " << candidates[i].amplitude << ")" << std::endl;
            }
        }
    }
    
    // Strategy 3: If we still need more peaks, relax the distance requirement
    if (selected.size() < 3) {
        for (size_t i = 1; i < candidates.size() && selected.size() < 3; ++i) {
            bool alreadySelected = false;
            
            for (const auto& selectedPeak : selected) {
                if (std::abs(candidates[i].timestamp - selectedPeak.timestamp) < 0.1) {
                    alreadySelected = true;
                    break;
                }
            }
            
            if (!alreadySelected) {
                selected.push_back(candidates[i]);
                if (m_verbose) {
                    std::cout << "      Selected additional peak: " << candidates[i].timestamp 
                              << "s (amp: " << candidates[i].amplitude << ")" << std::endl;
                }
            }
        }
    }
    
    // Sort selected peaks by timestamp for pattern creation
    std::sort(selected.begin(), selected.end(),
              [](const AudioPeak& a, const AudioPeak& b) {
                  return a.timestamp < b.timestamp;
              });
    
    return selected;
}

PeakPattern AudioSync::createPattern(const std::vector<AudioPeak>& peaks) {
    PeakPattern pattern;
    
    if (peaks.empty()) {
        return pattern;
    }
    
    pattern.startTime = peaks[0].timestamp;
    
    // Calculate intervals between consecutive peaks
    for (size_t i = 1; i < peaks.size(); ++i) {
        double interval = peaks[i].timestamp - peaks[i-1].timestamp;
        pattern.intervals.push_back(interval);
    }
    
    return pattern;
}

double AudioSync::matchPatterns(const PeakPattern& videoPattern,
                               const PeakPattern& audioPattern) {
    
    if (videoPattern.intervals.empty() || audioPattern.intervals.empty()) {
        if (m_verbose) {
            std::cout << "  ❌ Empty patterns, using first peak difference" << std::endl;
        }
        return audioPattern.startTime - videoPattern.startTime;
    }
    
    if (m_verbose) {
        std::cout << "  🔍 Finding best interval pattern match..." << std::endl;
    }
    
    // We need to reconstruct the actual peak times from the intervals
    std::vector<double> videoPeakTimes;
    std::vector<double> audioPeakTimes;
    
    // Reconstruct video peak times
    videoPeakTimes.push_back(videoPattern.startTime);
    double currentTime = videoPattern.startTime;
    for (double interval : videoPattern.intervals) {
        currentTime += interval;
        videoPeakTimes.push_back(currentTime);
    }
    
    // Reconstruct audio peak times
    audioPeakTimes.push_back(audioPattern.startTime);
    currentTime = audioPattern.startTime;
    for (double interval : audioPattern.intervals) {
        currentTime += interval;
        audioPeakTimes.push_back(currentTime);
    }
    
    if (m_verbose) {
        std::cout << "  📍 Video peak times: ";
        for (size_t i = 0; i < std::min(videoPeakTimes.size(), size_t(5)); ++i) {
            std::cout << videoPeakTimes[i] << "s ";
        }
        std::cout << std::endl;
        
        std::cout << "  📍 Audio peak times: ";
        for (size_t i = 0; i < std::min(audioPeakTimes.size(), size_t(5)); ++i) {
            std::cout << audioPeakTimes[i] << "s ";
        }
        std::cout << std::endl;
    }
    
    // Find the best starting position in audio pattern that matches video pattern
    double bestScore = 0.0;
    int bestAudioStart = 0;
    
    // Try matching video pattern starting at different positions in audio pattern
    for (int audioStart = 0; audioStart <= static_cast<int>(audioPattern.intervals.size()) - static_cast<int>(videoPattern.intervals.size()); ++audioStart) {
        
        double score = 0.0;
        int matches = 0;
        
        // Compare intervals starting from this position
        for (size_t i = 0; i < videoPattern.intervals.size() && (audioStart + i) < audioPattern.intervals.size(); ++i) {
            double videoInterval = videoPattern.intervals[i];
            double audioInterval = audioPattern.intervals[audioStart + i];
            
            double diff = std::abs(videoInterval - audioInterval);
            double tolerance = 0.3; // 300ms tolerance
            
            if (diff < tolerance) {
                matches++;
                score += (tolerance - diff) / tolerance; // Better match = higher score
            }
            
            // Check ratio as well
            if (videoInterval > 0.1 && audioInterval > 0.1) {
                double ratio = videoInterval / audioInterval;
                if (ratio > 0.8 && ratio < 1.2) {
                    score += 0.5; // Bonus for good ratio
                }
            }
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestAudioStart = audioStart;
        }
        
        if (m_verbose && score > 1.0) {
            std::cout << "    Audio start " << audioStart << ": score " << score 
                      << " (matches: " << matches << ")" << std::endl;
        }
    }
    
    if (m_verbose) {
        std::cout << "  🎯 Best pattern alignment: audio start " << bestAudioStart 
                  << " with score " << bestScore << std::endl;
    }
    
    // If we found a good pattern match, calculate the actual time offset
    if (bestScore > 1.0 && bestAudioStart < static_cast<int>(audioPeakTimes.size())) {
        // The first video peak should align with the audio peak at bestAudioStart
        double videoFirstPeak = videoPeakTimes[0];
        double audioAlignmentPeak = audioPeakTimes[bestAudioStart];
        
        double offset = audioAlignmentPeak - videoFirstPeak;
        
        if (m_verbose) {
            std::cout << "  📍 Video first peak: " << videoFirstPeak << "s" << std::endl;
            std::cout << "  📍 Audio alignment peak: " << audioAlignmentPeak << "s" << std::endl;
            std::cout << "  🎯 Calculated offset: " << offset << "s" << std::endl;
        }
        
        return offset;
    } else {
        // Fall back to first peak alignment
        double fallbackOffset = audioPattern.startTime - videoPattern.startTime;
        if (m_verbose) {
            std::cout << "  ⚠️  Pattern matching failed, using first peak alignment: " 
                      << fallbackOffset << "s" << std::endl;
        }
        return fallbackOffset;
    }
}

double AudioSync::calculatePatternSimilarity(const PeakPattern& pattern1,
                                            const PeakPattern& pattern2,
                                            double offset) {
    
    if (pattern1.intervals.empty() || pattern2.intervals.empty()) {
        return 0.0;
    }
    
    // The offset parameter doesn't matter for interval comparison!
    // Intervals are time-differences, not absolute times
    // We just compare the interval sequences directly
    
    double tolerance = 0.3; // 300ms tolerance for interval matching
    int matchedIntervals = 0;
    int totalComparisons = 0;
    
    // Compare interval patterns directly
    size_t maxComparisons = std::min(pattern1.intervals.size(), pattern2.intervals.size());
    
    for (size_t i = 0; i < maxComparisons; ++i) {
        double interval1 = pattern1.intervals[i];
        double interval2 = pattern2.intervals[i];
        
        double diff = std::abs(interval1 - interval2);
        
        if (diff < tolerance) {
            matchedIntervals++;
        }
        totalComparisons++;
        
        // Also check relative ratios (in case there's a tempo difference)
        if (interval1 > 0.1 && interval2 > 0.1) { // Avoid division by very small numbers
            double ratio = interval1 / interval2;
            if (ratio > 0.8 && ratio < 1.2) { // Within 20% ratio
                matchedIntervals++; // Bonus for good ratio match
            }
        }
    }
    
    // Calculate similarity score
    double score = totalComparisons > 0 ? 
        static_cast<double>(matchedIntervals) / (totalComparisons * 1.5) : 0.0;
    
    // Bonus for having more intervals to match
    if (totalComparisons >= 3) {
        score *= 1.2;
    }
    
    return std::min(1.0, score);
}

std::pair<double, double> AudioSync::calculateAnalysisWindow(
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
        return {10.0, 30.0};  // Default fallback
    }
    
    // Use middle portion of files
    double minDuration = std::min(videoDuration, audioDuration);
    double analysisDuration = std::min(30.0, minDuration * 0.3);  // 30% of file, max 30s
    analysisDuration = std::max(10.0, analysisDuration);  // Minimum 10s
    
    double startTime = (videoDuration - analysisDuration) / 2.0;
    startTime = std::max(0.0, startTime);
    
    return {startTime, analysisDuration};
}
/**
 * @file audio_sync.cpp
 * @brief Minimal audio sync implementation using peak detection
 */

#include "../include/audio_sync.h"
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
        std::cout << "\n🔍 Detecting sync offset..." << std::endl;
        std::cout << "  Video: " << videoFile.filename().string() << std::endl;
        std::cout << "  Audio: " << audioFile.filename().string() << std::endl;
    }
    
    // Calculate optimal analysis window
    auto [startTime, duration] = calculateAnalysisWindow(videoFile, audioFile);
    
    if (m_verbose) {
        std::cout << "  Analysis window: " << startTime << "s + " << duration << "s" << std::endl;
    }
    
    // Extract peaks from both files
    auto videoPeaks = extractPeaks(videoFile, startTime, duration);
    auto audioPeaks = extractPeaks(audioFile, startTime, duration);
    
    if (m_verbose) {
        std::cout << "  Video peaks: " << videoPeaks.size() << std::endl;
        std::cout << "  Audio peaks: " << audioPeaks.size() << std::endl;
    }
    
    if (videoPeaks.size() < 2 || audioPeaks.size() < 2) {
        if (m_verbose) {
            std::cout << "  ⚠️  Not enough peaks found, returning zero offset" << std::endl;
        }
        return 0.0;
    }
    
    // Find best offset
    double offset = matchPeaks(videoPeaks, audioPeaks);
    
    if (m_verbose) {
        std::cout << "  🎯 Best offset: " << offset << "s" << std::endl;
    }
    
    return offset;
}

std::vector<AudioPeak> AudioSync::extractPeaks(const std::filesystem::path& mediaFile,
                                              double startTime,
                                              double duration) {
    
    std::vector<AudioPeak> peaks;
    
    // Create temporary file for audio analysis
    std::string tempFile = "/tmp/audio_peaks_" + std::to_string(std::time(nullptr)) + 
                          "_" + std::to_string(rand() % 10000) + ".txt";
    
    // Use FFmpeg astats filter to find audio peaks
    std::ostringstream cmd;
    cmd << "ffmpeg -hide_banner -loglevel error ";
    cmd << "-ss " << startTime << " ";
    cmd << "-i \"" << mediaFile.string() << "\" ";
    cmd << "-t " << duration << " ";
    cmd << "-vn -af \"astats=metadata=1:reset=1\" ";
    cmd << "-f null - 2>&1 | grep 'Peak level' > \"" << tempFile << "\"";
    
    int result = std::system(cmd.str().c_str());
    if (result != 0) {
        if (m_verbose) {
            std::cout << "    ❌ Failed to extract audio stats" << std::endl;
        }
        return peaks;
    }
    
    // Alternative approach: Use volume detect to find loud sections
    std::string tempFile2 = "/tmp/audio_volume_" + std::to_string(std::time(nullptr)) + ".txt";
    
    std::ostringstream cmd2;
    cmd2 << "ffmpeg -hide_banner ";
    cmd2 << "-ss " << startTime << " ";
    cmd2 << "-i \"" << mediaFile.string() << "\" ";
    cmd2 << "-t " << duration << " ";
    cmd2 << "-vn -af \"volumedetect\" ";
    cmd2 << "-f null - 2>&1 | grep 'mean_volume\\|max_volume' > \"" << tempFile2 << "\"";
    
    std::system(cmd2.str().c_str());
    
    // For now, create synthetic peaks based on file analysis
    // This is a simplified approach - we'll detect actual peaks by analyzing volume over time
    
    // Simple peak detection using sliding window
    std::string tempAudio = "/tmp/audio_raw_" + std::to_string(std::time(nullptr)) + ".raw";
    
    std::ostringstream extractCmd;
    extractCmd << "ffmpeg -hide_banner -loglevel error ";
    extractCmd << "-ss " << startTime << " ";
    extractCmd << "-i \"" << mediaFile.string() << "\" ";
    extractCmd << "-t " << duration << " ";
    extractCmd << "-vn -f f32le -acodec pcm_f32le -ar 8000 -ac 1 ";
    extractCmd << "\"" << tempAudio << "\" 2>/dev/null";
    
    result = std::system(extractCmd.str().c_str());
    if (result == 0) {
        // Read the audio data and find peaks
        FILE* audioFile = fopen(tempAudio.c_str(), "rb");
        if (audioFile) {
            std::vector<float> samples;
            float sample;
            while (fread(&sample, sizeof(float), 1, audioFile) == 1) {
                samples.push_back(std::abs(sample));
            }
            fclose(audioFile);
            
            if (!samples.empty()) {
                // Find peaks in the audio envelope
                int windowSize = 400;  // ~50ms at 8kHz
                float threshold = 0.1f;
                
                for (size_t i = windowSize; i < samples.size() - windowSize; i += windowSize/4) {
                    // Calculate local energy
                    float energy = 0.0f;
                    for (int j = -windowSize/2; j < windowSize/2; ++j) {
                        energy += samples[i + j] * samples[i + j];
                    }
                    energy = std::sqrt(energy / windowSize);
                    
                    // Check if this is a peak
                    bool isPeak = true;
                    for (int j = -windowSize; j < windowSize; ++j) {
                        if (i + j >= 0 && i + j < samples.size()) {
                            float otherEnergy = 0.0f;
                            int start = std::max(0, static_cast<int>(i + j - windowSize/2));
                            int end = std::min(static_cast<int>(samples.size()), static_cast<int>(i + j + windowSize/2));
                            for (int k = start; k < end; ++k) {
                                otherEnergy += samples[k] * samples[k];
                            }
                            otherEnergy = std::sqrt(otherEnergy / (end - start));
                            
                            if (otherEnergy > energy * 1.2f) {
                                isPeak = false;
                                break;
                            }
                        }
                    }
                    
                    if (isPeak && energy > threshold) {
                        double timestamp = startTime + (i / 8000.0);  // Convert to seconds
                        peaks.emplace_back(timestamp, energy);
                        
                        if (m_verbose && peaks.size() <= 5) {
                            std::cout << "    Peak at " << timestamp << "s (amplitude: " << energy << ")" << std::endl;
                        }
                    }
                }
            }
        }
    }
    
    // Clean up temp files
    std::filesystem::remove(tempFile);
    std::filesystem::remove(tempFile2);
    std::filesystem::remove(tempAudio);
    
    // Sort peaks by timestamp
    std::sort(peaks.begin(), peaks.end(), 
              [](const AudioPeak& a, const AudioPeak& b) {
                  return a.timestamp < b.timestamp;
              });
    
    return peaks;
}

double AudioSync::matchPeaks(const std::vector<AudioPeak>& videoPeaks,
                            const std::vector<AudioPeak>& audioPeaks) {
    
    double bestOffset = 0.0;
    int bestMatches = 0;
    double tolerance = 0.5;  // 500ms tolerance
    
    // Test offsets from -15 to +15 seconds
    for (double offset = -15.0; offset <= 15.0; offset += 0.1) {
        int matches = 0;
        
        // Count how many video peaks have corresponding audio peaks with this offset
        for (const auto& videoPeak : videoPeaks) {
            double expectedTime = videoPeak.timestamp + offset;
            
            for (const auto& audioPeak : audioPeaks) {
                if (std::abs(audioPeak.timestamp - expectedTime) < tolerance) {
                    matches++;
                    break;  // Found a match for this video peak
                }
            }
        }
        
        if (matches > bestMatches) {
            bestMatches = matches;
            bestOffset = offset;
        }
    }
    
    if (m_verbose) {
        std::cout << "    Best match: " << bestMatches << " peaks at offset " << bestOffset << "s" << std::endl;
    }
    
    // If we found very few matches, try a simpler approach
    if (bestMatches < 2 && !videoPeaks.empty() && !audioPeaks.empty()) {
        // Just use the timing difference of the first strong peaks
        bestOffset = audioPeaks[0].timestamp - videoPeaks[0].timestamp;
        if (m_verbose) {
            std::cout << "    Fallback: using first peak difference = " << bestOffset << "s" << std::endl;
        }
    }
    
    return bestOffset;
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
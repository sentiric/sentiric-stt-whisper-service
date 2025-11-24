#pragma once
#include <vector>
#include <string>
#include <cstddef> // size_t için

struct AffectiveTags {
    std::string gender_proxy;   // "M" / "F"
    std::string emotion_proxy;  // "excited" | "neutral" | "sad" | "angry"
    float arousal = 0.0f;       // 0-1
    float valence = 0.0f;       // -1..1
    float pitch_mean = 0.0f;    // Hz
    float pitch_std = 0.0f;     // Hz
    float energy_mean = 0.0f;   // RMS
    float energy_std = 0.0f;
    float spectral_centroid = 0.0f;
    float zero_crossing_rate = 0.0f;
    std::vector<float> speaker_vec; // 8-D
};

// Optimization: Pass pointer + size instead of copying vector
AffectiveTags extract_prosody(const float* pcm_data, size_t n_samples, int sample_rate);
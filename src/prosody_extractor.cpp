#include "prosody_extractor.h"
#include <cmath>
#include <numeric>
#include <algorithm>

static float vector_mean(const std::vector<float>& v) {
    if (v.empty()) return 0;
    return std::accumulate(v.begin(), v.end(), 0.0f) / v.size();
}

static float vector_stdev(const std::vector<float>& v, float mean) {
    if (v.empty()) return 0;
    float acc = 0;
    for (float x : v) acc += (x - mean) * (x - mean);
    return std::sqrt(acc / v.size());
}

AffectiveTags extract_prosody(const std::vector<float>& pcm, int sample_rate) {
    AffectiveTags out;
    if (pcm.empty()) {
        out.gender_proxy = "?"; out.emotion_proxy = "neutral";
        out.pitch_mean = 150; out.pitch_std = 0;
        out.energy_mean = 0.05f; out.energy_std = 0;
        out.spectral_centroid = 0; out.zero_crossing_rate = 0.1f;
        out.arousal = 0.5f; out.valence = 0;
        out.speaker_vec.assign(8, 0.0f);
        return out;
    }
    const int frame_shift = sample_rate / 100; // 10 ms
    std::vector<float> f0s, rmses, zcrs, scs;
    for (size_t i = 0; i + 2 * frame_shift < pcm.size(); i += frame_shift) {
        // F0
        float r0 = 0, r1 = 0, r2 = 0;
        for (int k = 0; k < frame_shift; ++k) {
            float s = pcm[i + k];
            r0 += s * s; r1 += s * pcm[i + k + 1]; r2 += s * pcm[i + k + 2];
        }
        if (r0 < 1e-5f) continue;
        float m = r1 / (r0 + 1e-6f);
        float f0 = sample_rate / (2.0f * (m + 1e-6f));
        if (f0 > 80 && f0 < 400) f0s.push_back(f0);
        // RMS
        float rms = std::sqrt(r0 / frame_shift);
        rmses.push_back(rms);
        // ZCR
        int zcr = 0;
        for (int k = 1; k < frame_shift; ++k)
            if ((pcm[i + k] >= 0) != (pcm[i + k - 1] >= 0)) ++zcr;
        zcrs.push_back(static_cast<float>(zcr) / frame_shift);
        // Spectral centroid (kaba)
        float power = 0, weighted = 0;
        for (int k = 1; k < frame_shift; ++k) {
            float diff = std::abs(pcm[i + k] - pcm[i + k - 1]);
            weighted += diff * k; power += diff;
        }
        float sc = (power > 0) ? weighted / power : 0;
        scs.push_back(sc);
    }
    out.pitch_mean = f0s.empty() ? 150 : vector_mean(f0s);
    out.pitch_std  = f0s.empty() ? 0 : vector_stdev(f0s, out.pitch_mean);
    out.energy_mean = rmses.empty() ? 0.05f : vector_mean(rmses);
    out.energy_std  = rmses.empty() ? 0 : vector_stdev(rmses, out.energy_mean);
    out.spectral_centroid = scs.empty() ? 0 : vector_mean(scs);
    out.zero_crossing_rate = zcrs.empty() ? 0.1f : vector_mean(zcrs);

    // gender
    out.gender_proxy = (out.pitch_mean > 165.0f) ? "F" : "M";
    // arousal / valence
    out.arousal = std::min(1.0f, out.energy_mean * 20.0f);
    out.valence = (out.pitch_mean - 150.0f) / 100.0f;
    out.valence = std::max(-1.0f, std::min(1.0f, out.valence));
    // emotion
    if (out.arousal > 0.65f && out.valence > 0.4f)       out.emotion_proxy = "excited";
    else if (out.arousal > 0.65f && out.valence < -0.3f) out.emotion_proxy = "angry";
    else if (out.arousal < 0.35f)                         out.emotion_proxy = "sad";
    else                                                   out.emotion_proxy = "neutral";

    // 8-D speaker vec
    out.speaker_vec.resize(8);
    out.speaker_vec[0] = out.pitch_mean / 300.0f;
    out.speaker_vec[1] = out.pitch_std / 50.0f;
    out.speaker_vec[2] = out.energy_mean;
    out.speaker_vec[3] = out.energy_std;
    out.speaker_vec[4] = out.spectral_centroid / 1000.0f;
    out.speaker_vec[5] = out.zero_crossing_rate;
    out.speaker_vec[6] = out.arousal;
    out.speaker_vec[7] = (out.valence + 1.0f) / 2.0f;
    return out;
}
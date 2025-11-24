#include "prosody_extractor.h"
#include <cmath>
#include <numeric>
#include <algorithm>

// Yardımcılar
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

static float soft_norm(float val, float min_v, float max_v) {
    float norm = (val - min_v) / (max_v - min_v);
    return std::max(0.0f, std::min(1.0f, norm));
}

AffectiveTags extract_prosody(const float* pcm_data, size_t n_samples, int sample_rate) {
    AffectiveTags out;
    
    // Yetersiz veri kontrolü
    if (n_samples < 160 || pcm_data == nullptr) { 
        out.gender_proxy = "?"; out.emotion_proxy = "neutral";
        out.pitch_mean = 0; out.pitch_std = 0;
        out.energy_mean = 0; out.energy_std = 0;
        out.spectral_centroid = 0; out.zero_crossing_rate = 0;
        out.arousal = 0.0f; out.valence = 0.0f;
        out.speaker_vec.assign(8, 0.0f);
        return out;
    }

    const int frame_shift = sample_rate / 100; // 10 ms
    std::vector<float> f0s, rmses, zcrs, scs;
    size_t expected_frames = n_samples / frame_shift;
    f0s.reserve(expected_frames);
    rmses.reserve(expected_frames);
    zcrs.reserve(expected_frames);
    scs.reserve(expected_frames);

    int peak_count = 0; 
    float last_rms = 0;

    for (size_t i = 0; i + frame_shift <= n_samples; i += frame_shift) {
        // --- RMS ---
        float r0 = 0;
        for (int k = 0; k < frame_shift; ++k) {
            float val = pcm_data[i+k];
            r0 += val * val;
        }
        float rms = std::sqrt(r0 / frame_shift);
        rmses.push_back(rms);

        if (rms > 0.05f && last_rms <= 0.05f) peak_count++;
        last_rms = rms;

        // --- Zero Crossing ---
        int zcr = 0;
        for (int k = 1; k < frame_shift; ++k)
            if ((pcm_data[i + k] >= 0) != (pcm_data[i + k - 1] >= 0)) ++zcr;
        float zcr_val = static_cast<float>(zcr) / frame_shift;
        zcrs.push_back(zcr_val);

        // Pitch Proxy (ZCR based)
        if (zcr_val > 0.01f && zcr_val < 0.4f) {
            float estimated_f0 = zcr_val * sample_rate * 0.5f; 
            // Yetişkin insan sesi filtresi
            if(estimated_f0 > 60 && estimated_f0 < 450) f0s.push_back(estimated_f0);
        }

        // --- Spectral Centroid ---
        float power = 0, weighted = 0;
        for (int k = 1; k < frame_shift; ++k) {
            float diff = std::abs(pcm_data[i + k] - pcm_data[i + k - 1]);
            weighted += diff * k; power += diff;
        }
        float sc = (power > 0) ? weighted / power : 0;
        scs.push_back(sc);
    }

    out.pitch_mean = f0s.empty() ? 150.0f : vector_mean(f0s);
    out.pitch_std  = f0s.empty() ? 20.0f  : vector_stdev(f0s, out.pitch_mean);
    out.energy_mean = rmses.empty() ? 0.01f : vector_mean(rmses);
    out.energy_std  = rmses.empty() ? 0.00f : vector_stdev(rmses, out.energy_mean);
    out.spectral_centroid = scs.empty() ? 50.0f : vector_mean(scs);
    out.zero_crossing_rate = zcrs.empty() ? 0.1f : vector_mean(zcrs);

    // DÜZELTME: Oktav Hatası Giderici (Harmonic Correction Heuristic)
    // ZCR tabanlı pitch, harmonik zengin seslerde (erkek sesi) sıklıkla 2. harmoniği (2x pitch) yakalar.
    // Eğer pitch çok yüksekse (örn > 240Hz) ve standart sapma da yüksekse, bu muhtemelen bir oktav hatasıdır.
    if (out.pitch_mean > 240.0f) {
         // Yetişkinlerde 240Hz üstü konuşma nadirdir (çocuk hariç). 
         // Bunu güvenli tarafta kalmak için 0.5 ile çarpıyoruz (Oktav düşürme).
         out.pitch_mean *= 0.5f; 
    }

    float duration_sec = (float)n_samples / sample_rate;
    float speech_rate = (duration_sec > 0) ? (float)peak_count / duration_sec : 0.0f; 

    // Classification Rules (Updated Threshold)
    out.gender_proxy = (out.pitch_mean > 175.0f) ? "F" : "M";

    // Arousal & Valence...
    float norm_energy = soft_norm(out.energy_mean, 0.01f, 0.25f);
    float norm_rate = soft_norm(speech_rate, 2.0f, 8.0f);
    out.arousal = (norm_energy * 0.6f) + (norm_rate * 0.4f);

    float norm_pitch = soft_norm(out.pitch_mean, 80.0f, 320.0f);
    float norm_bright = soft_norm(out.spectral_centroid, 20.0f, 180.0f);
    out.valence = ((norm_pitch * 0.5f) + (norm_bright * 0.5f)) * 2.0f - 1.0f;

    if (out.arousal > 0.65f) out.emotion_proxy = (out.valence > 0.1f) ? "excited" : "angry";
    else if (out.arousal < 0.35f) out.emotion_proxy = (out.valence < -0.2f) ? "sad" : "neutral";
    else out.emotion_proxy = "neutral";

    out.speaker_vec.resize(8);
    out.speaker_vec[0] = soft_norm(out.pitch_mean, 50.0f, 350.0f);      
    out.speaker_vec[1] = soft_norm(out.pitch_std, 5.0f, 80.0f);         
    out.speaker_vec[2] = soft_norm(out.energy_mean, 0.0f, 0.3f);        
    out.speaker_vec[3] = soft_norm(out.spectral_centroid, 0.0f, 250.0f); 
    out.speaker_vec[4] = soft_norm(out.zero_crossing_rate, 0.0f, 0.5f); 
    out.speaker_vec[5] = soft_norm(speech_rate, 1.0f, 10.0f);           
    out.speaker_vec[6] = out.arousal;                                   
    out.speaker_vec[7] = (out.valence + 1.0f) / 2.0f;                   

    return out;
}
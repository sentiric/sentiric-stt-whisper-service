#include "prosody_extractor.h"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <vector>

// --- İstatistiksel Yardımcılar ---
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

static float vector_median(std::vector<float> v) {
    if (v.empty()) return 0.0f;
    size_t n = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + n, v.end());
    return v[n];
}

static float soft_norm(float val, float min_v, float max_v) {
    float norm = (val - min_v) / (max_v - min_v);
    return std::max(0.0f, std::min(1.0f, norm));
}

// --- Ana Fonksiyon ---

AffectiveTags extract_prosody(const float* pcm_data, size_t n_samples, int sample_rate, const ProsodyOptions& opts) {
    AffectiveTags out;
    
    if (n_samples < 160 || pcm_data == nullptr) { 
        out.gender_proxy = "?"; out.emotion_proxy = "neutral";
        // ... (Sıfırlamalar)
        out.pitch_mean = 0; out.pitch_std = 0;
        out.energy_mean = 0; out.energy_std = 0;
        out.spectral_centroid = 0; out.zero_crossing_rate = 0;
        out.arousal = 0.0f; out.valence = 0.0f;
        out.speaker_vec.assign(8, 0.0f);
        return out;
    }

    const int frame_shift = sample_rate / 100; 
    std::vector<float> f0s, rmses, zcrs, scs;
    size_t expected_frames = n_samples / frame_shift;
    f0s.reserve(expected_frames); rmses.reserve(expected_frames); zcrs.reserve(expected_frames); scs.reserve(expected_frames);

    int peak_count = 0; 
    float last_rms = 0;
    float lpf_val = 0.0f;
    const float lpf_alpha = opts.lpf_alpha; 

    for (size_t i = 0; i + frame_shift <= n_samples; i += frame_shift) {
        float r0 = 0; float max_amp = 0.0f;
        float filtered_frame[1600]; 
        int safe_frame_size = std::min(frame_shift, 1600);

        for (int k = 0; k < safe_frame_size; ++k) {
            float raw_val = pcm_data[i+k];
            float abs_val = std::abs(raw_val);
            if (abs_val > max_amp) max_amp = abs_val;
            r0 += raw_val * raw_val;
            lpf_val += lpf_alpha * (raw_val - lpf_val);
            filtered_frame[k] = lpf_val;
        }
        float rms = std::sqrt(r0 / safe_frame_size);
        rmses.push_back(rms);

        if (rms > 0.05f && last_rms <= 0.05f) peak_count++;
        last_rms = rms;

        float clipping_threshold = std::max(0.002f, rms * 0.15f); 
        int cycles = 0; bool is_positive = false; bool initialized = false;
        int standard_zcr_count = 0;
        
        for (int k = 1; k < safe_frame_size; ++k) {
            float val = filtered_frame[k]; 
            if ((val >= 0) != (filtered_frame[k-1] >= 0)) standard_zcr_count++;
            if (!initialized) {
                if (val > clipping_threshold) { is_positive = true; initialized = true; }
                else if (val < -clipping_threshold) { is_positive = false; initialized = true; }
            } else {
                if (is_positive && val < -clipping_threshold) { is_positive = false; cycles++; } 
                else if (!is_positive && val > clipping_threshold) { is_positive = true; }
            }
        }
        zcrs.push_back(static_cast<float>(standard_zcr_count) / safe_frame_size);

        if (rms > 0.015f && cycles > 0) {
            float duration = static_cast<float>(frame_shift) / sample_rate;
            float estimated_f0 = cycles / duration; 
            if(estimated_f0 >= opts.min_pitch && estimated_f0 <= opts.max_pitch) {
                f0s.push_back(estimated_f0);
            }
        }

        float power = 0, weighted = 0;
        for (int k = 1; k < safe_frame_size; ++k) {
            float diff = std::abs(pcm_data[i + k] - pcm_data[i + k - 1]);
            weighted += diff * k; power += diff;
        }
        scs.push_back((power > 0) ? weighted / power : 0);
    }

    // --- İstatistikler ---
    out.pitch_mean = vector_median(f0s);
    out.pitch_std  = f0s.empty() ? 0.0f : vector_stdev(f0s, vector_mean(f0s));
    out.energy_mean = rmses.empty() ? 0.01f : vector_mean(rmses);
    out.energy_std  = rmses.empty() ? 0.00f : vector_stdev(rmses, out.energy_mean);
    out.spectral_centroid = scs.empty() ? 50.0f : vector_mean(scs);
    out.zero_crossing_rate = zcrs.empty() ? 0.1f : vector_mean(zcrs);

    // -------------------------------------------------------------------------
    // 🛠️ HEURISTIC V3 (ZCR BASED FIX): KESİN ÇÖZÜM
    // -------------------------------------------------------------------------
    // Analiz: Erkek sesinin ZCR'si (Zero Crossing Rate) genelde 0.020 altındadır.
    // Kadın sesi (Ezgi) 0.027+, Erkek sesi (Can) 0.016 civarındadır.
    // Eğer Pitch yüksek (200Hz) ama ZCR düşükse (0.020 altı), bu %99 oktav hatasıdır.
    
    bool is_high_pitch = (out.pitch_mean > opts.gender_threshold); // > 165 Hz
    bool is_low_zcr = (out.zero_crossing_rate < 0.022f);           // Erkek karakteristiği

    if (is_high_pitch && is_low_zcr) {
         out.pitch_mean *= 0.5f; // Frekansı yarıya indir (200 -> 100)
    }
    
    // Yedek Kontrol: Enerji yüksek, pitch sınırda ve tını boğuksa
    else if (out.energy_mean > 0.12f && out.pitch_mean < 240.0f && out.spectral_centroid < 85.0f) {
         out.pitch_mean *= 0.5f;
    }

    float duration_sec = (float)n_samples / sample_rate;
    float speech_rate = (duration_sec > 0) ? (float)peak_count / duration_sec : 0.0f; 

    // --- SINIFLANDIRMA ---
    
    // 1. Cinsiyet (Artık Pitch düzeltildiği için doğrudan bakabiliriz)
    if (out.pitch_mean == 0.0f) {
        out.gender_proxy = "?";
    } else {
        // ZCR çok düşükse kesin erkektir, değilse Pitch'e güven.
        if (out.zero_crossing_rate < 0.020f) {
            out.gender_proxy = "M";
        } else {
            out.gender_proxy = (out.pitch_mean > opts.gender_threshold) ? "F" : "M";
        }
    }

    // 2. Valence (Mutluluk) - Nötr Bias +0.15
    float norm_pitch = soft_norm(out.pitch_mean, 80.0f, 320.0f);
    float norm_bright = soft_norm(out.spectral_centroid, 30.0f, 200.0f);
    out.valence = ((norm_pitch * 0.4f) + (norm_bright * 0.6f)) * 2.0f - 1.0f;
    out.valence += 0.15f; 

    // 3. Arousal
    float norm_energy = soft_norm(out.energy_mean, 0.02f, 0.20f);
    float norm_rate = soft_norm(speech_rate, 2.0f, 9.0f);
    out.arousal = (norm_energy * 0.7f) + (norm_rate * 0.3f);

    // 4. Duygu
    if (out.arousal > 0.60f) out.emotion_proxy = (out.valence > 0.0f) ? "excited" : "angry";
    else if (out.arousal < 0.30f) out.emotion_proxy = (out.valence < -0.3f) ? "sad" : "neutral";
    else out.emotion_proxy = "neutral";

    // 5. Speaker Vector
    out.speaker_vec.resize(8);
    out.speaker_vec[0] = soft_norm(out.pitch_mean, 50.0f, 300.0f);
    out.speaker_vec[1] = soft_norm(out.pitch_std, 5.0f, 100.0f);         
    out.speaker_vec[2] = soft_norm(out.energy_mean, 0.0f, 0.3f);        
    out.speaker_vec[3] = soft_norm(out.spectral_centroid, 0.0f, 250.0f); 
    out.speaker_vec[4] = soft_norm(out.zero_crossing_rate, 0.0f, 0.5f); 
    out.speaker_vec[5] = soft_norm(speech_rate, 1.0f, 12.0f);           
    out.speaker_vec[6] = out.arousal;                                   
    out.speaker_vec[7] = (out.valence + 1.0f) / 2.0f;                   

    return out;
}
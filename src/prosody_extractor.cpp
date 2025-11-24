#include "prosody_extractor.h"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <vector>

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

// YENİ: Outlier korumalı medyan hesaplayıcı
static float vector_median(std::vector<float> v) {
    if (v.empty()) return 0.0f;
    size_t n = v.size() / 2;
    // Hızlı seçim algoritması (Tam sıralama yapmaz, O(n) çalışır)
    std::nth_element(v.begin(), v.begin() + n, v.end());
    return v[n];
}

static float soft_norm(float val, float min_v, float max_v) {
    float norm = (val - min_v) / (max_v - min_v);
    return std::max(0.0f, std::min(1.0f, norm));
}

AffectiveTags extract_prosody(const float* pcm_data, size_t n_samples, int sample_rate) {
    AffectiveTags out;
    
    if (n_samples < 160 || pcm_data == nullptr) { 
        out.gender_proxy = "?"; out.emotion_proxy = "neutral";
        out.pitch_mean = 0; out.pitch_std = 0;
        out.energy_mean = 0; out.energy_std = 0;
        out.spectral_centroid = 0; out.zero_crossing_rate = 0;
        out.arousal = 0.0f; out.valence = 0.0f;
        out.speaker_vec.assign(8, 0.0f);
        return out;
    }

    const int frame_shift = sample_rate / 100; // 10 ms (16kHz için 160 sample)
    std::vector<float> f0s, rmses, zcrs, scs;
    size_t expected_frames = n_samples / frame_shift;
    f0s.reserve(expected_frames);
    rmses.reserve(expected_frames);
    zcrs.reserve(expected_frames);
    scs.reserve(expected_frames);

    int peak_count = 0; 
    float last_rms = 0;

    for (size_t i = 0; i + frame_shift <= n_samples; i += frame_shift) {
        float r0 = 0;
        float max_amp = 0.0f;

        // 1. RMS ve Peak Amplitude Hesapla
        for (int k = 0; k < frame_shift; ++k) {
            float val = pcm_data[i+k];
            float abs_val = std::abs(val);
            if (abs_val > max_amp) max_amp = abs_val;
            r0 += val * val;
        }
        float rms = std::sqrt(r0 / frame_shift);
        rmses.push_back(rms);

        // Hece sayacı (Enerji patlamaları)
        if (rms > 0.05f && last_rms <= 0.05f) peak_count++;
        last_rms = rms;

        // 2. Center Clipped ZCR (Schmitt Trigger Logic)
        // Gürültüyü önlemek için sadece belirli bir genliği (threshold) aşan dalgaları say.
        // Bu, temel frekansı (Pitch) bulmak için basit ZCR'dan çok daha üstündür.
        // Eşik: O karedeki max sesin %30'u veya genel gürültü tabanı.
        float clipping_threshold = std::max(0.01f, max_amp * 0.30f); 
        
        int cycles = 0;
        bool is_positive = false; // State machine
        bool initialized = false;

        // Standart ZCR için (Feature olarak kalsın ama pitch için kullanmayacağız)
        int standard_zcr_count = 0; 

        for (int k = 1; k < frame_shift; ++k) {
            float val = pcm_data[i+k];
            
            // Standart ZCR
            if ((val >= 0) != (pcm_data[i+k-1] >= 0)) standard_zcr_count++;

            // Robust Pitch Detection Logic
            if (!initialized) {
                if (val > clipping_threshold) { is_positive = true; initialized = true; }
                else if (val < -clipping_threshold) { is_positive = false; initialized = true; }
            } else {
                if (is_positive && val < -clipping_threshold) {
                    is_positive = false; // Zero cross downwards with significant amplitude
                    cycles++; 
                } else if (!is_positive && val > clipping_threshold) {
                    is_positive = true; // Zero cross upwards
                }
            }
        }
        
        float zcr_val = static_cast<float>(standard_zcr_count) / frame_shift;
        zcrs.push_back(zcr_val);

        // 3. Pitch Calculation (Frequency = Cycles / Time)
        // RMS kontrolü: Sadece konuşma olan yerlerde pitch ara
        if (rms > 0.02f && cycles > 0) {
            float duration = static_cast<float>(frame_shift) / sample_rate; // örn 0.01s
            float estimated_f0 = cycles / duration; // örn 1 döngü / 0.01s = 100Hz

            // Human Voice Range Filter (50Hz - 500Hz)
            if(estimated_f0 >= 60.0f && estimated_f0 <= 450.0f) {
                f0s.push_back(estimated_f0);
            }
        }

        // 4. Spectral Centroid
        float power = 0, weighted = 0;
        for (int k = 1; k < frame_shift; ++k) {
            float diff = std::abs(pcm_data[i + k] - pcm_data[i + k - 1]);
            weighted += diff * k; power += diff;
        }
        float sc = (power > 0) ? weighted / power : 0;
        scs.push_back(sc);
    }

    // --- İstatistikler ve Stabilizasyon ---
    
    // DÜZELTME: Aritmetik Ortalama yerine MEDYAN kullanımı.
    // Bu, anlık oktav hatalarını (örn: 120Hz giderken bir anda 240Hz ölçülmesi) yok sayar.
    out.pitch_mean = vector_median(f0s); 
    
    // Standart sapma hala aritmetik ortalamaya göre hesaplanır (değişkenliği görmek için)
    out.pitch_std  = f0s.empty() ? 0.0f : vector_stdev(f0s, vector_mean(f0s));
    
    out.energy_mean = rmses.empty() ? 0.01f : vector_mean(rmses);
    out.energy_std  = rmses.empty() ? 0.00f : vector_stdev(rmses, out.energy_mean);
    out.spectral_centroid = scs.empty() ? 50.0f : vector_mean(scs);
    out.zero_crossing_rate = zcrs.empty() ? 0.1f : vector_mean(zcrs);

    float duration_sec = (float)n_samples / sample_rate;
    float speech_rate = (duration_sec > 0) ? (float)peak_count / duration_sec : 0.0f; 

    // --- Classification Rules ---
    // Eğer medyan pitch 0 ise (hiç geçerli döngü bulunamadıysa)
    if (out.pitch_mean < 50.0f) {
        // Fallback: Spectral Centroid'e göre tahmin et (Yedek Plan)
        // Kadın sesleri genelde daha yüksek frekans bileşenlerine sahiptir.
        // Spectral centroid > 70 (empirik değer) genellikle kadın veya çocuktur.
        if (out.spectral_centroid > 80.0f) out.gender_proxy = "F";
        else if (out.energy_mean > 0.01) out.gender_proxy = "M"; // Enerji var ama pitch yoksa (Deep male fry)
        else out.gender_proxy = "?";
    } else {
        // Eşik değeri: 175Hz (Erkek ort: 120Hz, Kadın ort: 210Hz)
        out.gender_proxy = (out.pitch_mean > 175.0f) ? "F" : "M";
    }

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
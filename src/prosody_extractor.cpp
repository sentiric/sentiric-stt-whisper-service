#include "prosody_extractor.h"
#include <cmath>
#include <numeric>
#include <algorithm>

// Yardımcı: Vektör ortalaması
static float vector_mean(const std::vector<float>& v) {
    if (v.empty()) return 0;
    return std::accumulate(v.begin(), v.end(), 0.0f) / v.size();
}

// Yardımcı: Standart sapma
static float vector_stdev(const std::vector<float>& v, float mean) {
    if (v.empty()) return 0;
    float acc = 0;
    for (float x : v) acc += (x - mean) * (x - mean);
    return std::sqrt(acc / v.size());
}

// Yardımcı: Soft Clamp (0-1 arasına yumuşak sıkıştırma)
static float soft_norm(float val, float min_v, float max_v) {
    float norm = (val - min_v) / (max_v - min_v);
    return std::max(0.0f, std::min(1.0f, norm));
}

AffectiveTags extract_prosody(const std::vector<float>& pcm, int sample_rate) {
    AffectiveTags out;
    
    // Boş veya çok kısa segment kontrolü
    if (pcm.size() < 160) { // 10ms altı
        out.gender_proxy = "?"; out.emotion_proxy = "neutral";
        out.pitch_mean = 0; out.pitch_std = 0;
        out.energy_mean = 0; out.energy_std = 0;
        out.spectral_centroid = 0; out.zero_crossing_rate = 0;
        out.arousal = 0.0f; out.valence = 0.0f;
        out.speaker_vec.assign(8, 0.0f);
        return out;
    }

    const int frame_shift = sample_rate / 100; // 10 ms pencereler
    std::vector<float> f0s, rmses, zcrs, scs;
    
    for (size_t i = 0; i + 2 * frame_shift < pcm.size(); i += frame_shift) {
        // --- Pitch (Zero-Crossing tabanlı basit yaklaşım + Autocorrelation approx) ---
        float r0 = 0, r1 = 0;
        for (int k = 0; k < frame_shift; ++k) {
            float s = pcm[i + k];
            r0 += s * s; 
            r1 += s * pcm[i + k + 1]; 
        }
        
        // Basit F0 tahmini
        if (r0 > 1e-5f) {
            float m = r1 / (r0 + 1e-6f);
            // m, 1'e yakınsa düşük frekans, -1'e yakınsa yüksek frekans (kabaca)
            // Bu çok kaba bir proxy'dir, ancak CPU maliyeti sıfırdır.
            // Daha iyi bir proxy için Zero Crossing aralık ortalaması:
            float f0 = sample_rate / (2.0f * (std::acos(m) / 3.14159f) * frame_shift); 
            // Üstteki matematiksel olarak hatalı olabilir, autocorrelation basitleştirmesi:
            // Biz burada SttEngine içinde YIN/PYIN kullanamayız (çok ağır).
            // Onun yerine Zero Crossing Rate'den türetilmiş pitch proxy kullanalım.
        }

        // --- RMS Energy ---
        float rms = std::sqrt(r0 / frame_shift);
        rmses.push_back(rms);

        // --- Zero Crossing Rate ---
        int zcr = 0;
        for (int k = 1; k < frame_shift; ++k)
            if ((pcm[i + k] >= 0) != (pcm[i + k - 1] >= 0)) ++zcr;
        float zcr_val = static_cast<float>(zcr) / frame_shift;
        zcrs.push_back(zcr_val);

        // --- Spectral Centroid Proxy (Difference Magnitude) ---
        float power = 0, weighted = 0;
        for (int k = 1; k < frame_shift; ++k) {
            float diff = std::abs(pcm[i + k] - pcm[i + k - 1]);
            weighted += diff * k; 
            power += diff;
        }
        float sc = (power > 0) ? weighted / power : 0;
        scs.push_back(sc);
        
        // --- Pitch Proxy (Robust Zero Crossing Period) ---
        // ZCR üzerinden temel frekans tahmini (monofonik ses için makul)
        if (zcr_val > 0.01f && zcr_val < 0.5f) {
            float estimated_f0 = zcr_val * sample_rate * 0.5f; 
            if(estimated_f0 > 50 && estimated_f0 < 400) f0s.push_back(estimated_f0);
        }
    }

    // İstatistikler
    out.pitch_mean = f0s.empty() ? 150.0f : vector_mean(f0s);
    out.pitch_std  = f0s.empty() ? 20.0f  : vector_stdev(f0s, out.pitch_mean);
    out.energy_mean = rmses.empty() ? 0.01f : vector_mean(rmses);
    out.energy_std  = rmses.empty() ? 0.00f : vector_stdev(rmses, out.energy_mean);
    out.spectral_centroid = scs.empty() ? 50.0f : vector_mean(scs);
    out.zero_crossing_rate = zcrs.empty() ? 0.1f : vector_mean(zcrs);

    // --- Rule-Based Classification ---
    // Gender (Pitch Mean Eşiği)
    // 165Hz genellikle erkek/kadın ayrımı için kaba bir eşiktir.
    out.gender_proxy = (out.pitch_mean > 175.0f) ? "F" : "M";

    // Arousal (Enerji + Pitch Değişkenliği)
    // Yüksek enerji ve yüksek pitch değişkenliği = Yüksek Arousal (Heyecan/Öfke)
    float norm_energy = soft_norm(out.energy_mean, 0.01f, 0.2f);
    float norm_p_std  = soft_norm(out.pitch_std, 10.0f, 80.0f);
    out.arousal = (norm_energy * 0.7f) + (norm_p_std * 0.3f);

    // Valence (Pitch Mean + Spectral Balance) - Çok zor bir tahmin
    // Yüksek pitch genellikle daha pozitif algılanır (mutluluk), düşük ve boğuk ses negatif.
    float norm_pitch = soft_norm(out.pitch_mean, 80.0f, 300.0f);
    float norm_bright = soft_norm(out.spectral_centroid, 20.0f, 150.0f);
    out.valence = ((norm_pitch * 0.6f) + (norm_bright * 0.4f)) * 2.0f - 1.0f; // -1..1 scale

    // Emotion Mapping
    if (out.arousal > 0.6f) {
        out.emotion_proxy = (out.valence > 0.0f) ? "excited" : "angry";
    } else if (out.arousal < 0.3f) {
        out.emotion_proxy = (out.valence < -0.2f) ? "sad" : "neutral";
    } else {
        out.emotion_proxy = "neutral";
    }

    // --- 8-Dimensional Speaker Vector Construction ---
    // Bu vektör Cosine Similarity için kullanılacak. Normalizasyon kritik.
    out.speaker_vec.resize(8);
    
    // 1. Pitch Mean (Normalize: 50Hz - 350Hz)
    out.speaker_vec[0] = soft_norm(out.pitch_mean, 50.0f, 350.0f);
    
    // 2. Pitch Std (Normalize: 0Hz - 100Hz) - Entonasyon karakteristiği
    out.speaker_vec[1] = soft_norm(out.pitch_std, 0.0f, 100.0f);
    
    // 3. Energy Mean (Normalize: 0.0 - 0.3) - Ses gürlüğü
    out.speaker_vec[2] = soft_norm(out.energy_mean, 0.0f, 0.3f);
    
    // 4. Energy Std (Normalize: 0.0 - 0.1) - Dinamik aralık
    out.speaker_vec[3] = soft_norm(out.energy_std, 0.0f, 0.1f);
    
    // 5. Spectral Centroid (Normalize: 0 - 200) - Ses rengi (Timbre)
    out.speaker_vec[4] = soft_norm(out.spectral_centroid, 0.0f, 200.0f);
    
    // 6. Zero Crossing Rate (Normalize: 0 - 0.5) - Boğukluk/Keskinlik
    out.speaker_vec[5] = soft_norm(out.zero_crossing_rate, 0.0f, 0.5f);
    
    // 7. Arousal (Zaten 0-1)
    out.speaker_vec[6] = out.arousal;
    
    // 8. Valence (Normalized to 0-1)
    out.speaker_vec[7] = (out.valence + 1.0f) / 2.0f;

    return out;
}
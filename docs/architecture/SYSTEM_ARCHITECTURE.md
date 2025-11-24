# 🏗️ Sistem Mimarisi (v2.5.0)

Sentiric STT Whisper Service, ses verisini sadece metne çevirmekle kalmaz, aynı zamanda konuşmacının kimliğini ve duygu durumunu da analiz eder.

## 1. Veri Akış Şeması

```mermaid
graph TD
    Client[Client / Omni-Studio] -->|WAV/PCM| Server[STT Service]
    
    subgraph "C++ Backend"
        Server --> Pre[Preprocessing & VAD]
        Pre -->|Speech Segments| Whisper[Whisper Inference]
        Pre -->|Raw PCM| DSP[Prosody Extractor]
        
        Whisper -->|Tokens & Timestamps| Merger[Result Merger]
        
        subgraph "DSP Engine"
            DSP --> LPF[Low Pass Filter]
            LPF --> Stats[Pitch/ZCR/Energy Calc]
            Stats --> Heuristic[ZCR Gender Check & Octave Fix]
            Heuristic --> Emotion[Adaptive Emotion Mapping]
            Heuristic --> Vector[Vector Polarization]
        end
        
        Vector --> Cluster[Speaker Clusterer (0.94 Threshold)]
        
        DSP --> Merger
        Cluster --> Merger
    end
    
    Merger -->|Enriched JSON| Client
```

## 2. Kritik Algoritmalar

### A. ZCR-Based Gender Correction
```cpp
bool is_low_zcr = (out.zero_crossing_rate < 0.024f); // Erkek İmzası
if (is_high_pitch && is_low_zcr) {
    out.pitch_mean *= 0.5f; // Oktav düzeltme
    out.gender_proxy = "M"; // Cinsiyet zorlama
}
```

### B. Speaker Diarization Strategy
Sistem, `whisper.cpp`'nin `tdrz` (tinydiarize) özelliğini **kullanmaz**. Bunun yerine kendi DSP vektörlerini kullanır:
1.  Her segment için 8 boyutlu (Pitch, Energy, Spectral vb.) bir öznitelik vektörü çıkarılır.
2.  Cinsiyete göre vektör uzayı manipüle edilir (Polarization).
3.  `0.94` Cosine Similarity eşiği ile kümeleme yapılır.


---

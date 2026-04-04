# 🧬 STT & Affective Intelligence (Duyuşsal Zeka) Logic

Bu belge, Whisper motorunun harici bir yapay zeka (LLM) modeli kullanmadan, saf C++ içinde matematiksel sinyal işleme (DSP) ile çıkardığı "Duyuşsal Zeka" metriklerinin bilimsel altyapısını açıklar.

## 1. Dahili Veri Akışı (Internal Pipeline)
```mermaid
graph TD
    Input[gRPC Audio Stream] --> Filter[Low Pass Filter & Resampler]
    Filter --> Whisper[Whisper Engine GPU]
    Filter --> DSP[Prosody Extractor CPU]
    
    subgraph "Affective Engine (Zero-Latency Math)"
        DSP --> Math[Pitch & Energy Calc]
        Math --> ZCR[ZCR Octave Correction]
        ZCR --> Gender[Gender Classification]
        Gender --> Vector[Vector Polarization]
    end
    
    Whisper -->|Tokens & Text| JSON[Final Response]
    Vector -->|Speaker ID| JSON
    Gender -->|Emotion Proxy| JSON
```

## 2. Oktav Hatası (Octave Error) ve Cinsiyet Düzeltme
Basit frekans (Pitch) analizleri, kalın erkek seslerindeki 2. harmoniği ana frekans sanıp erkeği kadın olarak (Örn: 100Hz yerine 200Hz) algılayabilir.
*   **Heuristic Çözüm:** `prosody_extractor.cpp` içinde ZCR (Zero Crossing Rate) kontrolü yapılır.
*   **Magic Number (0.024):** Ses Yüksek Frekans (Kadın) çıksa bile, eğer `ZCR < 0.024` ise frekans zorla yarıya indirilir ve cinsiyet `M` (Erkek) yapılır. Bu değer binlerce testle optimize edilmiştir.

## 3. Vector Polarization (Kimlik Ayrıştırma / Diarization)
Farklı cinsiyetten kişilerin ses frekansları uzayda birbirine yakın düşerse, `SpeakerClusterer` onları aynı kişi sanıp birleştirebilir.
*   **Algoritma:** Cinsiyet `M` ise, vektörün Pitch bileşeni `[0.0 - 0.4]` arasına sıkıştırılır. `F` ise `[0.6 - 1.0]` arasına itilir. Bu "Kutuplaştırma (Polarization)", Cosine Similarity algoritmasının farklı cinsiyetleri %100 ayırmasını sağlar.

## 4. Agresif Halüsinasyon Filtresi
Whisper sessizlikte (Noise) altyazı üretmeye meyillidir. 
*   **Algoritma:** Eğer üretilen segmentin ortalama Token Olasılığı (Probability) `%40`'ın altındaysa VEYA metin `[Yasaklı Kelimeler]` (Örn: "Altyazı", "Teşekkürler", "Abone ol") içeriyorsa, `is_hallucination` fonksiyonu bu metni yutar ve dışarıya sessizlik döner.

# 🎧 Sentiric STT Whisper Service (v2.5.0)

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-stt-whisper-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-stt-whisper-service/actions/workflows/build-and-push.yml)

**Sentiric STT**, OpenAI Whisper modelini kullanan, **C++ tabanlı**, GPU hızlandırmalı ve **Duyuşsal Zeka (Affective Intelligence)** yeteneklerine sahip yüksek performanslı bir konuşmadan yazıya mikroservisidir.

## 🚀 Özellikler (v2.5.0)

*   **⚡ Native Performans:** Python bağımlılığı yok. `whisper.cpp` v1.8.2 motoru ile ultra düşük gecikme.
*   **🧠 Hibrit Mimari:** 
    *   **VAD:** CPU (Silero VAD v5).
    *   **Inference:** GPU (NVIDIA CUDA + Flash Attention).
*   **🎭 Zero-Latency Affective DSP:** Ek model yüklemeyen, sinyal işleme tabanlı duygu ve kimlik analizi:
    *   **Cinsiyet Tespiti:** ZCR (Zero Crossing Rate) ve Pitch analizi ile %95+ doğruluk.
    *   **Duygu Haritalama:** Arousal/Valence uzayında sesin enerjisine ve tınısına göre anlık duygu tahmini.
    *   **Akıllı Diarization:** "Vector Polarization" tekniği ile konuşmacıları (Kadın/Erkek) kesin olarak ayırır.
*   **🔄 Dynamic Batching:** Çoklu istekleri (Parallel Requests) GPU'da paralel işler.
*   **🎛️ Omni-Studio v8.2:** Entegre Web UI ile Karaoke modu, canlı metrikler ve detaylı DSP ayarları.

---

## 🛠️ Hızlı Başlangıç

### Ön Gereksinimler
*   Docker & Docker Compose
*   (Opsiyonel) NVIDIA GPU & Container Toolkit

### 1. Çalıştırma (GPU)
```bash
make up-gpu
```
*Servis ilk açılışta gerekli modelleri (~1.5GB) otomatik indirir.*

### 2. Test Etme (Omni-Studio)
Tarayıcınızda **`http://localhost:15030`** adresine gidin.
*   Mikrofon ile kayıt yapın (Hands-Free VAD desteği).
*   Dosya yükleyin.
*   Karaoke modu ile kelime kelime takibi yapın.

### 3. API Kullanımı
```bash
curl http://localhost:15030/v1/transcribe \
  -F "file=@audio.wav" \
  -F "language=tr"
```

---

## ⚙️ Yapılandırma (v2.5 Default)

| Değişken | Varsayılan | Açıklama |
| :--- | :--- | :--- |
| `STT_WHISPER_SERVICE_MODEL_FILENAME` | `ggml-medium.bin` | Whisper modeli. |
| `STT_WHISPER_SERVICE_PARALLEL_REQUESTS` | `2` | GPU batch boyutu. |
| `STT_WHISPER_SERVICE_ENABLE_DIARIZATION`| `true` | Konuşmacı ayrıştırma. |
| `STT_WHISPER_SERVICE_PITCH_GATE` | `170` | (UI) Cinsiyet ayrımı için temel frekans eşiği. |
| `STT_WHISPER_SERVICE_CLUSTER_THRESHOLD` | `0.94` | (UI) Konuşmacı kümeleme hassasiyeti. |

---

## 🏗️ Mimari

```mermaid
graph TD
    Input[Audio Input] --> Resampler[Resampler 16kHz]
    Resampler --> VAD[Silero VAD (CPU)]
    VAD --> Whisper[Whisper Encoder (GPU)]
    
    subgraph "DSP & Affective Engine"
        PCM[PCM Data] --> Pitch[Pitch/ZCR Analysis]
        Pitch --> Correction[Octave Error Correction]
        Correction --> Gender[Gender Classification]
        Gender --> Emotion[Relative Emotion Mapping]
        Gender --> Vector[Vector Polarization]
    end
    
    Whisper --> Tokens[Text Tokens]
    Tokens --> JSON[Final JSON Response]
    Vector --> JSON
    Emotion --> JSON
```

## 📜 Lisans
AGPLv3 License.


---

# 🎧 Sentiric STT Whisper Service (v2.5.1)

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-stt-whisper-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-stt-whisper-service/actions/workflows/build-and-push.yml)

**Sentiric STT**, OpenAI Whisper modelini kullanan, **C++ tabanlı**, GPU hızlandırmalı ve **Duyuşsal Zeka (Affective Intelligence)** yeteneklerine sahip yüksek performanslı bir konuşmadan yazıya mikroservisidir.

## 🚀 Özellikler (v2.5.1)

*   **⚡ Native Performans:** Python bağımlılığı yok. `whisper.cpp` v1.8.2 motoru ile ultra düşük gecikme.
*   **🧠 Hibrit Mimari:** 
    *   **VAD:** CPU (Silero VAD v5).
    *   **Inference:** GPU (NVIDIA CUDA + Flash Attention).
*   **🛡️ Production Ready:**
    *   **Backpressure:** Kaynaklar dolduğunda sistemi kilitlemek yerine isteği reddeder (Fail-Fast).
    *   **Security:** Non-root kullanıcı ile çalışır.
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

### 3. API Kullanımı
```bash
curl http://localhost:15030/v1/transcribe \
  -F "file=@audio.wav" \
  -F "language=tr"
```

---

## ⚙️ Yapılandırma (v2.5.1 Default)

| Değişken | Varsayılan | Açıklama |
| :--- | :--- | :--- |
| `STT_WHISPER_SERVICE_MODEL_FILENAME` | `ggml-medium.bin` | Whisper modeli. (small, medium, large-v3) |
| `STT_WHISPER_SERVICE_PARALLEL_REQUESTS` | `2` | Aynı anda işlenecek ses sayısı (GPU VRAM'e göre artırın). |
| `STT_WHISPER_SERVICE_QUEUE_TIMEOUT_MS` | `5000` | **(YENİ)** Havuz doluysa en fazla kaç ms beklensin? |
| `STT_WHISPER_SERVICE_ENABLE_DIARIZATION`| `true` | Konuşmacı ayrıştırma. |
| `STT_WHISPER_SERVICE_CLUSTER_THRESHOLD` | `0.94` | **(YENİ)** Konuşmacı ayrım hassasiyeti (Düşük=Birleştirir, Yüksek=Ayırır). |
| `STT_WHISPER_SERVICE_PITCH_GATE` | `170` | Cinsiyet ayrımı için temel frekans eşiği (Hz). |

---

## 🏗️ Mimari

```mermaid
graph TD
    Input[Audio Input] --> Resampler[Resampler 16kHz]
    Resampler --> VAD[Silero VAD (CPU)]
    VAD --> Queue[Request Queue (Timeout Protected)]
    Queue --> Whisper[Whisper Encoder (GPU)]
    
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

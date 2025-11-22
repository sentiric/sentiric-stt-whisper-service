# 🎧 Sentiric STT Whisper Service (v2.2.0)

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-stt-whisper-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-stt-whisper-service/actions/workflows/build-and-push.yml)

**Sentiric STT**, OpenAI Whisper modelini kullanan, **C++ tabanlı**, GPU hızlandırmalı, akıllı kaynak yönetimine sahip, yüksek performanslı bir konuşmadan yazıya (Speech-to-Text) mikroservisidir.

## 🚀 Özellikler (v2.2.0)

*   **⚡ Native Performans:** Python bağımlılığı yok. `whisper.cpp` v1.8.2 motoru ile ultra düşük gecikme ve bellek kullanımı.
*   **🧠 Hibrit Mimari:** 
    *   **VAD (Sessizlik Tespiti):** CPU üzerinde çalışır (Silero VAD v5). Kaynak tasarrufu sağlar.
    *   **Inference (Transkripsiyon):** GPU (NVIDIA CUDA) üzerinde çalışır. `Flash Attention` aktiftir.
*   **🔄 Dynamic Batching:** Aynı anda gelen birden fazla isteği (Parallel Requests) GPU üzerinde paralel işler.
*   **🗣️ Speaker Diarization:** Konuşmacı değişimlerini tespit eder (Experimental).
*   **📝 Context Prompting:** Halüsinasyonları önlemek veya terim öğretmek için modele ipucu (prompt) verilebilir.
*   **📦 Auto-Provisioning:** Model dosyalarını (Whisper & VAD) başlangıçta otomatik indirir. Manuel işlem gerektirmez.
*   **🎛️ Omni-Studio:** Entegre Web UI ile tarayıcı üzerinden test, VAD ayarı ve görselleştirme.

---

## 🛠️ Hızlı Başlangıç

### Ön Gereksinimler
*   Docker & Docker Compose
*   (Opsiyonel) NVIDIA GPU & Container Toolkit

### 1. Çalıştırma (GPU)
```bash
make up-gpu
```
*Servis ilk açılışta gerekli modelleri (~1.5GB) otomatik indirecektir. Logları izleyin.*

### 2. Test Etme (Omni-Studio)
Tarayıcınızda **`http://localhost:15030`** adresine gidin.
*   Mikrofon ile kayıt yapın.
*   Dosya yükleyin.
*   Prompt (İpucu) girerek sonucu yönlendirin.

### 3. API Kullanımı
```bash
curl http://localhost:15030/v1/transcribe \
  -F "file=@audio.wav" \
  -F "language=tr" \
  -F "prompt=Altyazı ekleme."
```

---

## ⚙️ Yapılandırma (Docker Compose)

Ana ayarlar `docker-compose.yml` üzerinden yönetilir:

| Değişken | Varsayılan | Açıklama |
| :--- | :--- | :--- |
| `STT_WHISPER_SERVICE_MODEL_FILENAME` | `ggml-medium.bin` | Kullanılacak Whisper modeli (tiny, base, small, medium, large-v3). |
| `STT_WHISPER_SERVICE_PARALLEL_REQUESTS` | `2` | GPU'da aynı anda işlenecek istek sayısı. VRAM'e göre artırın. |
| `STT_WHISPER_SERVICE_ENABLE_VAD` | `true` | Silero VAD aktif/pasif. |
| `STT_WHISPER_SERVICE_ENABLE_DIARIZATION`| `true` | Konuşmacı ayrıştırma aktif/pasif. |
| `STT_WHISPER_SERVICE_FLASH_ATTN` | `true` | GPU Flash Attention optimizasyonu. |

---

## 🏗️ Mimari

```mermaid
graph TD
    Client[Client / Gateway] -->|HTTP/gRPC| API[API Layer]
    API -->|Audio| Resampler[Resampler (16kHz)]
    Resampler -->|Float32| VAD[Silero VAD (CPU)]
    
    VAD -- Silence --> Discard[Discard]
    VAD -- Speech --> Queue[State Pool Queue]
    
    Queue -->|Batch| GPU[Whisper Engine (CUDA)]
    GPU -->|Tokens| Decoder[Decoder & Diarization]
    Decoder -->|JSON| Client
```

## 📜 Lisans
AGPLv3 License. `whisper.cpp` ve `ggml` kütüphanelerine dayanır.


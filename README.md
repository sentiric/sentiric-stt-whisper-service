# 🤫 Sentiric STT Whisper Service (C++ Edition)

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml)

**Sentiric STT Whisper Service**, OpenAI'ın Whisper modelini kullanan, yüksek performanslı, düşük gecikmeli ve kaynak dostu bir **C++ Mikroservisidir**.

Bu proje, önceki Python tabanlı servisin **v2.0.0 (Native)** sürümü olarak, `whisper.cpp` motoru üzerine yeniden inşa edilmiştir.

## 🚀 Neden C++?

| Özellik | Eski (Python) | Yeni (C++) |
| :--- | :--- | :--- |
| **Mimari** | Python + FasterWhisper | Native C++ + Whisper.cpp |
| **Docker İmajı** | ~4.5 GB | **~200 MB** |
| **Eşzamanlılık** | GIL ile sınırlı | **Gerçek Multi-Threading** |
| **Soğuk Başlangıç** | 3-5 saniye | **< 100ms** |
| **Bağımlılıklar** | Karmaşık (pip, venv) | **Stabil (vcpkg, static link)** |

## 🛠️ Kurulum ve Çalıştırma

### Ön Gereksinimler
*   Docker & Docker Compose
*   (Opsiyonel) NVIDIA GPU & Container Toolkit

### 1. Modelleri İndirin
Servisi başlatmadan önce gerekli Whisper modelini indirmelisiniz:

```bash
# Varsayılan (base) modeli indirir
./scripts/download_models.sh base

# Veya belirli bir modeli indirin (tiny, small, medium, large-v3)
./scripts/download_models.sh medium
```

### 2. Servisi Başlatın (Docker)

**CPU Modu:**
```bash
make up-cpu
```

**GPU Modu (NVIDIA):**
```bash
make up-gpu
```

Servis şu adreslerde aktif olacaktır:
*   **HTTP Health Check:** `http://localhost:15030/health`
*   **Prometheus Metrics:** `http://localhost:15030/metrics`
*   **gRPC Server:** `localhost:15031`

---

## 🧪 CLI ile Test Etme

Dahili CLI aracı ile servisi test edebilirsiniz.

```bash
# Konteynere bağlanıp CLI'ı çalıştırır
docker compose exec stt-whisper-service stt_cli file /path/to/audio.wav

# Streaming testi
docker compose exec stt-whisper-service stt_cli stream /path/to/audio.wav
```

---

## ⚙️ Yapılandırma

Servis, ortam değişkenleri ile yapılandırılır (Bkz: `docker-compose.yml`):

| Değişken | Açıklama | Varsayılan |
|---|---|---|
| `STT_WHISPER_SERVICE_MODEL_FILENAME` | Kullanılacak model dosyası (`ggml-base.bin` vb.) | `ggml-base.bin` |
| `STT_WHISPER_SERVICE_THREADS` | İşlemci thread sayısı | `4` |
| `STT_WHISPER_SERVICE_LANGUAGE` | Hedef dil (`auto`, `tr`, `en`) | `auto` |
| `STT_WHISPER_SERVICE_TRANSLATE` | İngilizceye çeviri yapılsın mı? | `false` |

---

## 🏗️ Geliştirme

```bash
# Yerel derleme (Linux/WSL)
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
make -j
```

**Lisans:** AGPL-3.0

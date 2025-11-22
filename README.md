# 🤫 Sentiric STT Whisper Service (C++ Edition)

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-stt-whisper-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-stt-whisper-service/actions/workflows/build-and-push.yml)

**Sentiric STT Whisper Service**, OpenAI'ın Whisper modelini kullanan, yüksek performanslı, düşük gecikmeli ve kaynak dostu bir **C++ Mikroservisidir**.

Bu proje, `whisper.cpp` motoru üzerine inşa edilmiş **v2.1.0 (Native)** sürümüdür.

## 🚀 Neden C++?

| Özellik | Eski (Python) | Yeni (C++ v2.1) |
| :--- | :--- | :--- |
| **Mimari** | Python + FasterWhisper | Native C++ + Whisper.cpp |
| **Docker İmajı** | ~4.5 GB | **~200 MB** (CPU) / ~2.4GB (GPU) |
| **Eşzamanlılık** | GIL ile sınırlı | **Gerçek Multi-Threading** |
| **Detay** | Sadece Metin | **Kelime Bazlı Zaman Damgası & Olasılık** |
| **Soğuk Başlangıç** | 3-5 saniye | **< 100ms** |

## 🛠️ Kurulum ve Çalıştırma

### Ön Gereksinimler
*   Docker & Docker Compose
*   (Opsiyonel) NVIDIA GPU & Container Toolkit

### Hızlı Başlat (GPU)
```bash
make up-gpu
```
*Servis otomatik olarak gerekli modeli indirecektir.*

Servis şu adreslerde aktif olacaktır:
*   **Omni-Studio (UI):** `http://localhost:15030`
*   **Health Check:** `http://localhost:15030/health`
*   **Prometheus Metrics:** `http://localhost:15032/metrics`
*   **gRPC Server:** `localhost:15031`

---

## 🧪 CLI ile Test Etme

Dahili CLI aracı ile servisi test edebilirsiniz.

```bash
# Konteynere bağlanıp CLI'ı çalıştırır
docker compose exec stt-whisper-service stt_cli file /app/jfk.wav
```

Daha fazla detay için [docs/FEATURES.md](docs/FEATURES.md) dosyasına bakın.

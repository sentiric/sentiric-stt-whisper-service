# 🎧 Sentiric STT Whisper Service

[![Production Ready](https://img.shields.io/badge/status-production%20ready-success.svg)]()
[![Architecture](https://img.shields.io/badge/arch-C%2B%2B17_Native-blue.svg)]()

**Sentiric STT**, OpenAI Whisper (C++ Port) motorunu kullanan, GPU hızlandırmalı ve yerleşik **Duyuşsal Zeka (Affective Intelligence)** yeteneklerine sahip konuşmadan yazıya (Speech-to-Text) servisidir.

## 🚀 Hızlı Başlangıç

### 1. Çalıştırma (Docker)
```bash
# GPU (NVIDIA CUDA) ile - Önerilen
docker run -d --gpus all -p 15030:15030 -p 15031:15031 ghcr.io/sentiric/sentiric-stt-whisper-service:latest-gpu
```

### 2. Doğrulama (Health Check)
```bash
curl http://localhost:15030/health
```

## 🏛️ Mimari Anayasa ve Kılavuzlar
* **Kodlama Kuralları (AI/İnsan):** Bu repoya katkıda bulunmadan önce GİZLİ [.context.md](.context.md) dosyasını okuyun.
* **Sinyal Matematiği (DSP):** Duygu ve Cinsiyet analiz algoritmaları için [LOGIC.md](LOGIC.md) dosyasını okuyun.
* **Sistem Sınırları ve Topoloji:** Bu servisin platformdaki rolü, portları ve iletişimde olduğu servisler **[sentiric-spec](https://github.com/sentiric/sentiric-spec)** anayasasında tanımlıdır.
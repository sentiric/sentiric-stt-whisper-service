# 🆔 Proje Kimliği ve Mimari Bağlam

**Servis Adı:** `sentiric-stt-whisper-service`
**Versiyon:** v2.1.0 (Feature Release)
**Rol:** AI Engine Layer (Ses İşleme Motoru)

## 🎯 Amaç ve Sorumluluk
Bu servis, Sentiric platformunun **"Kulaklarıdır"**. 
Ham ses verisini (PCM/WAV) alır, işler (Resampling/VAD) ve metne (Transcript) dönüştürür. 
Üst katman servisleri (Gateway, Agent) için düşük seviyeli, yüksek performanslı bir işlem birimidir.

## 🏗️ Teknik Mimari (v2.1)
Eski Python (v1) mimarisinden, performans ve kaynak verimliliği için **Native C++** mimarisine geçilmiştir.

*   **Motor:** `whisper.cpp` (OpenAI Whisper C++ Portu) + Token Level Timestamps
*   **Protokol:** gRPC (Streaming & Unary) + HTTP (Health & Metrics)
*   **Bağımlılıklar:** `vcpkg` (Paket Yöneticisi), `CMake` (Build), `Docker` (Runtime).
*   **Donanım:** CPU (AVX2) ve GPU (NVIDIA CUDA) hibrit destek.

## 🔌 Entegrasyon Noktaları
1.  **Girdi:** `stt-gateway-service` veya `agent-service` üzerinden gRPC ile ses alır.
2.  **Çıktı:** Metin (Text), Dil (Language), Olasılık (Probability), Zaman Damgaları (Segment & Word Level) döner.
3.  **Gözlemlenebilirlik:** Prometheus (`/metrics`) üzerinden RTF (Real-Time Factor) ve Latency verisi sunar.

## 📂 Kritik Dizinler
*   `src/stt_engine.*`: Whisper sarmalayıcı ve ses işleme mantığı.
*   `src/model_manager.*`: Otomatik model indirme ve doğrulama.
*   `proto/`: gRPC kontratları (Source of Truth: `sentiric-contracts`).
*   `models/`: İndirilen GGML model dosyaları (Git'e dahil edilmez).
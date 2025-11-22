# 🆔 Proje Kimliği ve Mimari Bağlam

**Servis Adı:** `sentiric-stt-whisper-service`
**Versiyon:** v2.2.0 (Production Release)
**Rol:** AI Engine Layer (Ses İşleme Motoru)

## 🎯 Amaç ve Sorumluluk
Bu servis, Sentiric platformunun **"Kulaklarıdır"**. 
Ham ses verisini (PCM/WAV) alır, işler (Resampling/VAD) ve metne (Transcript) dönüştürür. 
Üst katman servisleri (Gateway, Agent) için düşük seviyeli, yüksek performanslı bir işlem birimidir.

## 🏗️ Teknik Mimari (v2.2)
Eski Python (v1) mimarisinden, performans ve kaynak verimliliği için **Native C++** mimarisine geçilmiştir.

*   **Motor:** `whisper.cpp` v1.8.2 (OpenAI Whisper C++ Portu)
*   **Performans:** Dynamic Batching (Parallel Requests) + Flash Attention.
*   **Stabilite:** Hybrid VAD (CPU) + Inference (GPU).
*   **Protokol:** gRPC (Streaming & Unary) + HTTP (Health & Metrics)
*   **Bağımlılıklar:** `vcpkg` (Paket Yöneticisi), `CMake` (Build), `Docker` (Runtime).

## 🔌 Entegrasyon Noktaları
1.  **Girdi:** `stt-gateway-service` veya `agent-service` üzerinden gRPC ile ses alır.
2.  **Çıktı:** Metin (Text), Dil, Olasılık, Zaman Damgaları ve **Konuşmacı Değişimi (Diarization)** bilgisi döner.
3.  **Gözlemlenebilirlik:** Prometheus (`/metrics`) üzerinden RTF (Real-Time Factor) ve Latency verisi sunar.

## 📂 Kritik Dizinler
*   `src/stt_engine.*`: Whisper sarmalayıcı, Batching ve VAD mantığı.
*   `src/model_manager.*`: Auto-Provisioning (Otomatik indirme).
*   `proto/`: gRPC kontratları.
*   `studio/`: Omni-Studio Web UI kaynak kodları.
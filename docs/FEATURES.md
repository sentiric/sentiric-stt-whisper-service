# 🌟 Sistem Özellikleri ve Teknik Yetenekler

Bu belge, **Sentiric STT Whisper Service (v2.1.0)** tarafından sağlanan tüm teknik özellikleri, desteklenen protokolleri ve operasyonel yetenekleri listeler.

## 🧠 1. Çekirdek Motor (Core Engine)
Servisin kalbinde, OpenAI Whisper modelinin yüksek performanslı C++ portu (`whisper.cpp`) çalışır.

*   **Native C++ Mimarisi:** Python bağımlılığı yoktur. Doğrudan makine kodunda çalışır.
*   **Hibrit Hesaplama:**
    *   **CPU:** AVX2 optimizasyonu ve INT8 quantization ile her işlemcide çalışır.
    *   **GPU:** NVIDIA CUDA desteği ile FP16 hassasiyetinde yüksek hız.
*   **Model Yönetimi (Auto-Provisioning):**
    *   Başlangıçta model dosyasını (`ggml-medium.bin` vb.) kontrol eder.
    *   Eksikse HuggingFace üzerinden otomatik indirir.
    *   Bozuk dosyaları (hash/boyut kontrolü) tespit eder ve onarır.
*   **Ses İşleme:**
    *   `libsamplerate` ile 8kHz/44.1kHz -> 16kHz otomatik dönüşüm (High Quality Resampling).
    *   32-bit Float normalizasyonu.

## 📡 2. API ve Protokoller
Dış dünya ile iletişim için iki ana kapı sunar.

### A. gRPC (Yüksek Performans)
Mikroservisler arası iletişim (Internal) için tasarlanmıştır.
*   **Streaming:** Canlı ses akışını (chunk-by-chunk) alır ve işler.
*   **Unary:** Tekil ses dosyalarını işler.
*   **Strict Contracts:** `sentiric-contracts` (Protobuf) ile tip güvenliği.

### B. HTTP REST (Entegrasyon)
Web istemcileri ve 3. parti araçlar için tasarlanmıştır.
*   **OpenAI Uyumluluğu:** `/v1/audio/transcriptions` endpoint'i, OpenAI kütüphaneleriyle (LangChain, AutoGPT vb.) doğrudan çalışır.
*   **Detaylı Çıktı:** JSON yanıtında sadece metin değil, şu detaylar da döner:
    *   Kelime bazlı zaman damgaları (Word-Level Timestamps).
    *   Token güven skorları (Confidence/Probability).
    *   Segment başlangıç/bitiş süreleri.
*   **Sentiric Metadata:** Yanıt başlıklarında veya gövdesinde işlem süresi ve RTF (Real-Time Factor) bilgisi.

## 🎛️ 3. Omni-Studio (Web UI)
Servis içinde gömülü (embedded) olarak gelen test ve geliştirme arayüzü.
*   **Hands-Free Mode:** Tarayıcı tabanlı VAD (Voice Activity Detection) ile konuşmayı otomatik algılar ve gönderir.
*   **Real-time Visualizer:** Ses dalgalarını (waveform) canlı görselleştirir.
*   **Payload Inspector:** Dönen JSON verisini ham haliyle inceleme imkanı.
*   **Latency Metrics:** İşlem süresi, ağ gecikmesi ve model hızını panelde gösterir.

## 📊 4. Gözlemlenebilirlik (Observability)
Production ortamları için telemetri verileri sağlar.
*   **Prometheus Metrics (`/metrics`):**
    *   `stt_requests_total`: Toplam istek sayısı.
    *   `stt_audio_seconds_processed_total`: İşlenen toplam ses süresi (saniye).
    *   `stt_request_latency_seconds`: İstek başına işlem süresi histogramı.
*   **Health Checks:** Kubernetes/Docker için Liveness ve Readiness probe desteği.
*   **Structured Logging:** `spdlog` ile seviyeli (INFO, WARN, ERROR) ve renkli loglama.

## 🛠️ 5. Dağıtım ve DevOps
*   **Docker First:** 
    *   `Dockerfile` (CPU Optimized ~200MB)
    *   `Dockerfile.gpu` (CUDA Runtime ~4GB)
*   **vcpkg Entegrasyonu:** Tüm C++ kütüphaneleri (gRPC, Protobuf, nlohmann_json) statik olarak derlenir.
*   **Cross-Platform:** Linux (Ubuntu 22.04+) ve Windows (WSL2) tam uyumluluk.
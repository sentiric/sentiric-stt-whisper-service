# 🌟 Sistem Özellikleri ve Teknik Yetenekler

Bu belge, **Sentiric STT Whisper Service (v2.2.0)** tarafından sağlanan tüm teknik özellikleri listeler.

## 🧠 1. Çekirdek Motor (Core Engine)
*   **Native C++ Mimarisi:** Python bağımlılığı yoktur. `whisper.cpp v1.8.2` çekirdeği üzerinde çalışır.
*   **Hibrit Hesaplama (Hybrid Compute):**
    *   **CPU (VAD):** Silero VAD v5, hafif olduğu için CPU üzerinde çalışarak GPU kaynaklarını korur.
    *   **GPU (Inference):** Transkripsiyon işlemleri NVIDIA CUDA ve Flash Attention optimizasyonu ile yapılır.
*   **Dynamic Batching:** Aynı anda gelen çoklu istekleri (Parallel Requests) "State Pooling" mimarisiyle GPU üzerinde paralel işler.
*   **Auto-Provisioning:** Başlangıçta eksik modelleri (Whisper & VAD) GitHub LFS üzerinden otomatik indirir ve doğrular.

## 🗣️ 2. Zeka ve Doğruluk
*   **Context Prompting (Bağlam):** Modele "başlangıç ipucu" verilerek özel isimlerin (örn: Sentiric, Tıbbi terimler) doğru yazılması sağlanır.
*   **Speaker Diarization:** Ses dosyasındaki konuşmacı değişim noktalarını (`speaker_turn_next`) tespit eder.
*   **Hallucination Control:** Prompting ve Confidence Filter ile sessizlik anlarındaki uydurma metinleri (örn: "Altyazı M.K.") engeller.

## 📡 3. API ve Protokoller
### A. gRPC (Internal - High Performance)
*   **Streaming:** Canlı ses akışını (chunk-by-chunk) işler.
*   **Unary:** Tekil dosya transkripsiyonu.
*   **Strict Contracts:** `sentiric-contracts` (Protobuf) ile tip güvenliği.

### B. HTTP REST (Integration)
*   **OpenAI Uyumluluğu:** `/v1/audio/transcriptions` endpoint'i ile standart kütüphanelerle çalışır.
*   **Parametreler:** `file` (WAV), `language` (Dil), `prompt` (İpucu).
*   **Zengin Çıktı:** JSON yanıtında metin, kelime bazlı zaman damgaları, olasılık skorları ve konuşmacı bilgisi döner.

## 🎛️ 4. Omni-Studio (Web UI)
Servis ile birlikte gelen entegre test ve yönetim arayüzü.

*   **Prompt Input:** Arayüz üzerinden modele anlık direktif ve kelime listesi verme imkanı.
*   **Interactive Playback:** Transkribe edilen ses kayıtlarını tarayıcı üzerinde tekrar dinleme (`<audio>` player).
*   **Speaker Visualization:** Konuşmacı değişimlerini görsel olarak ayırır ("🗣️ KONUŞMACI DEĞİŞİMİ").
*   **Real-time Visualizer:** Mikrofon girişini canlı dalga formu (waveform) olarak gösterir.
*   **Hands-Free Mode:** Tarayıcı tabanlı VAD ile konuşmayı otomatik algılar ve gönderir.
*   **Performance Metrics:** İşlem süresi, RTF (Hız Faktörü) ve güven skorlarını gösterir.

## 📊 5. Gözlemlenebilirlik
*   **Prometheus Metrics (`/metrics`):** Toplam istek, işlenen ses süresi (saniye), gecikme histogramları.
*   **Health Checks:** Kubernetes Liveness/Readiness için `/health` endpoint'i (Model durumu kontrolü dahil).
*   **Structured Logging:** Renkli ve seviyeli (INFO/WARN/ERROR) konsol logları.
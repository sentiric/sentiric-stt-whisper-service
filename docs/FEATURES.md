# 🌟 Sistem Özellikleri ve Teknik Yetenekler

Bu belge, **Sentiric STT Whisper Service (v2.2.0)** tarafından sağlanan tüm teknik özellikleri listeler.

## 🧠 1. Çekirdek Motor (Core Engine)
*   **Native C++ Mimarisi:** Python bağımlılığı yoktur. `whisper.cpp v1.8.2` çekirdeği.
*   **Hibrit Hesaplama:**
    *   **CPU:** VAD (Silero v5) işlemleri CPU'da yapılarak GPU meşguliyeti önlenir.
    *   **GPU:** Transkripsiyon işlemleri NVIDIA CUDA + Flash Attention ile yapılır.
*   **Dynamic Batching:** Aynı anda gelen çoklu istekleri (Parallel Requests) tek bir Model Context üzerinde paralel işler (State Pooling).
*   **Auto-Provisioning:** Başlangıçta eksik modelleri (Whisper & VAD) otomatik indirir ve doğrular.

## 🗣️ 2. Zeka ve Doğruluk
*   **Context Prompting:** Modele "başlangıç ipucu" verilerek özel isimlerin (örn: Sentiric) doğru yazılması sağlanır ve halüsinasyonlar engellenir.
*   **Speaker Diarization:** Ses dosyasındaki konuşmacı değişim noktalarını (`speaker_turn_next`) tespit eder.
*   **Hallucination Filter:** Düşük olasılıklı segmentleri ve sessizlik anlarındaki uydurmaları filtreler.

## 📡 3. API ve Protokoller
### A. gRPC (Internal)
*   **Streaming:** Canlı ses akışını işler.
*   **Unary:** Tekil dosya işler.

### B. HTTP REST (External)
*   **OpenAI Uyumluluğu:** `/v1/audio/transcriptions` endpoint'i.
*   **Parametreler:** `file`, `language`, `prompt`.
*   **Detaylı Çıktı:** Kelime bazlı zaman damgaları, olasılıklar ve konuşmacı bilgisi.

## 🎛️ 4. Omni-Studio (Web UI)
*   **Prompt Input:** Arayüzden modele direktif verme imkanı.
*   **Real-time Visualizer:** Canlı ses dalgası görselleştirme.
*   **Hands-Free Mode:** Tarayıcı tabanlı VAD ile otomatik kayıt.
*   **Latency Metrics:** RTF (Real Time Factor) ve işlem süresi analizi.

## 📊 5. Gözlemlenebilirlik
*   **Prometheus Metrics:** İstek sayıları, toplam ses süresi, gecikme histogramları.
*   **Structured Logging:** Renkli ve seviyeli (INFO/WARN/ERROR) konsol logları.
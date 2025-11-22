# 📋 Görev ve Yol Haritası

## ✅ TAMAMLANAN (FAZ 1 & 2 - MIGRATION & STABILITY)
- [x] **Altyapı:** CMake, vcpkg ve Docker altyapısının kurulması.
- [x] **Motor:** `whisper.cpp` entegrasyonu ve `SttEngine` sınıfı.
- [x] **Native VAD:** `Silero-VAD` entegrasyonu ile sessiz bölümlerin GPU'ya gönderilmeden filtrelenmesi.
- [x] **Sunucu:** gRPC (Streaming/Unary) ve HTTP sunucularının yazılması.
- [x] **Ses İşleme:** `libsamplerate` ile 8kHz -> 16kHz otomatik dönüşüm.
- [x] **Auto-Provisioning:** Servis başladığında modelin otomatik indirilmesi (`ModelManager`).
- [x] **Sentiric Omni-Studio:** Tarayıcı tabanlı test arayüzü (VAD, Visualizer).
- [x] **Advanced Features:** Word-Level Timestamps ve Token Probability desteği.
- [x] **Observability:** Prometheus metrikleri ve doğru port yapılandırması.

## ⏳ AKTİF (FAZ 3 - OPTIMIZATION)
- [ ] **Dynamic Batching:** Aynı anda gelen isteklerin GPU'da paralel işlenmesi (Throughput artışı).
- [ ] **Speaker Diarization:** Konuşmacı ayrıştırma (Kim konuştu?).

## 🔮 GELECEK (FAZ 4 - SCALE)
- [ ] **Distributed Inference:** Birden fazla GPU/Node üzerinde yük dağılımı.
- [ ] **Custom Vocabulary:** Sektörel terimlerin modele öğretilmesi (Prompting).
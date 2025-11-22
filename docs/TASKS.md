# 📋 Görev ve Yol Haritası

## ✅ TAMAMLANAN (FAZ 1 - MIGRATION)
- [x] **Altyapı:** CMake, vcpkg ve Docker altyapısının kurulması.
- [x] **Motor:** `whisper.cpp` entegrasyonu ve `SttEngine` sınıfı.
- [x] **Sunucu:** gRPC (Streaming/Unary) ve HTTP sunucularının yazılması.
- [x] **Ses İşleme:** `libsamplerate` ile 8kHz -> 16kHz otomatik dönüşüm.
- [x] **CI/CD:** GitHub Actions ile otomatik Docker imajı (CPU/GPU) üretimi.

## ⏳ AKTİF (FAZ 2 - SELF-SUFFICIENCY)
- [ ] **Auto-Provisioning:** Servis başladığında modelin otomatik indirilmesi (`ModelManager`).
- [ ] **Sentiric Omni-Studio:** Tarayıcı tabanlı test arayüzü (Microphone, Drag&Drop).

## 🔮 GELECEK (FAZ 3 - ADVANCED)
- [ ] **Dynamic Batching:** Aynı anda gelen isteklerin GPU'da paralel işlenmesi.
- [ ] **Speaker Diarization:** Konuşmacı ayrıştırma (Kim konuştu?).
- [ ] **Word-Level Timestamps:** Kelime bazlı zaman damgası.
# 📋 Görev ve Yol Haritası

## ✅ TAMAMLANAN (FAZ 1, 2 & 3 - STABILITY & OPTIMIZATION)
- [x] **Altyapı:** CMake, vcpkg ve Docker altyapısının kurulması.
- [x] **Motor:** `whisper.cpp` entegrasyonu (v1.8.2) ve `SttEngine` sınıfı.
- [x] **Native VAD:** `Silero-VAD` (v5.1.2) entegrasyonu. CPU üzerinde çalıştırılarak Segfault sorunları giderildi.
- [x] **Dynamic Batching:** `State Pooling` ile aynı anda çoklu istek (Parallel Request) desteği.
- [x] **Auto-Provisioning:** `ModelManager` ile eksik modellerin (Ana Model + VAD) otomatik indirilmesi.
- [x] **Speaker Diarization (v2.2):** `tdrz_enable` ile konuşmacı değişimi tespiti (Experimental).
- [x] **Observability:** Prometheus metrikleri ve detaylı loglama.

## ⏳ AKTİF (FAZ 4 - SCALE & INTELLIGENCE)
- [ ] **Custom Vocabulary:** Sektörel terimlerin modele öğretilmesi (Prompting).
- [ ] **Distributed Inference:** Birden fazla GPU/Node üzerinde yük dağılımı.

## 🔮 GELECEK
- [ ] **Streaming Diarization:** Canlı akışta konuşmacı ayrıştırma.
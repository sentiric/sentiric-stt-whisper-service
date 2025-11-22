# 📋 Görev ve Yol Haritası

## ✅ TAMAMLANAN (FAZ 1, 2 & 3 - STABILITY & OPTIMIZATION)
- [x] **Altyapı:** CMake, vcpkg ve Docker altyapısının kurulması.
- [x] **Motor:** `whisper.cpp` entegrasyonu (v1.8.2) ve `SttEngine` sınıfı.
- [x] **Native VAD:** `Silero-VAD` (v5.1.2) entegrasyonu. CPU üzerinde çalıştırılarak Segfault sorunları giderildi.
- [x] **Dynamic Batching:** `State Pooling` ile aynı anda çoklu istek (Parallel Request) desteği.
- [x] **Auto-Provisioning:** `ModelManager` ile eksik modellerin otomatik indirilmesi.
- [x] **Speaker Diarization:** `tdrz_enable` ile konuşmacı değişimi tespiti.
- [x] **Context Prompting:** API ve UI üzerinden modele bağlam (ipucu) verme yeteneği.
- [x] **Observability:** Prometheus metrikleri ve detaylı loglama.

## ⏳ AKTİF (FAZ 4 - SCALE & INTELLIGENCE)
- [ ] **Distributed Inference:** Birden fazla GPU/Node üzerinde yük dağılımı (Kubernetes Scale-Out).
- [ ] **Fine-Tuning Pipeline:** Sektörel verilerle modelin eğitilmesi (LoRA).

## 🔮 GELECEK
- [ ] **Streaming Diarization:** Canlı akışta anlık konuşmacı ayrıştırma.
- [ ] **Audio Enhancement:** Gürültü engelleme ön işlemcisi.
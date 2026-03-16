# Dosya: docs/TASKS.md

# 📋 Görev ve Yol Haritası (Post-v2.5.0)

## ✅ TAMAMLANAN (MILESTONE v2.5.0 - CORE STABILITY)
- [x] **Engine:** Whisper.cpp v1.8.2 entegrasyonu + Flash Attention.
- [x] **Performance:** Dynamic Batching (State Pooling) ile paralel GPU işleme.
- [x] **DSP (Heuristic):** Harici model olmadan Cinsiyet, Duygu ve Speaker Vector analizi.
    - [x] ZCR tabanlı "Oktav Hatası" düzeltmesi (0.024 Threshold).
    - [x] Cinsiyete göre vektör kutuplaştırma (Vector Polarization).
    - [x] Cinsiyete göre normalize edilmiş Duygu Analizi (Adaptive Emotion).
- [x] **UI (Omni-Studio):** Scoped Karaoke, Canlı TPS Grafiği, Persistent Config.
- [x] **Doc:** FEATURES.md teknik anayasasının oluşturulması.
- [x] **Architecture:** Enforced JSON logging and `trace_id` propagation in gRPC to strictly comply with `constraints.yaml` specification.

---

## 🚀 AKTİF FAZ: OPERASYONEL ÖLÇEKLENME (FAZ 4)
Kod tabanı stabil. Şimdi bu motoru "Enterprise" seviyesinde ölçeklenebilir hale getirmeliyiz.


---

## 🔮 GELECEK FAZ: MODEL UZMANLAŞMASI (FAZ 5)
UI'daki "Domain" butonlarını gerçek yapay zeka eğitimi ile güçlendirmek.

- [ ] **LoRA Adapter Support:** C++ motoruna Runtime'da LoRA (Low-Rank Adaptation) yükleme yeteneği.
    - *Amaç:* Ana modeli değiştirmeden "Tıp", "Hukuk" modüllerini tak-çıkar yapmak.
- [ ] **Automated Benchmarking:** "Golden Dataset" ile her sürümde WER (Word Error Rate) ve Cinsiyet Doğruluk oranının otomatik ölçülmesi.
- [ ] **Audio Enhancement:** Gürültülü kayıtlar için Whisper öncesi `RNNoise` veya `DeepFilterNet` entegrasyonu (C++ seviyesinde).

---

## 🐛 BACKLOG & IMPROVEMENTS
- [ ] **UI:** Mobil görünümde waveform canvas performans optimizasyonu.
- [ ] **Backend:** `libsamplerate` yerine daha hızlı bir resampler (örn: speex) değerlendirmesi.
# 💡 KB-04: Whisper.cpp v1.8.2 Yükseltme Araştırması ve Etki Analizi

**DURUM:** Mevcut proje `v1.7.1` sürümünü kullanmaktadır.
**HEDEF:** `v1.8.2` (Latest Stable) sürümüne geçiş.
**TARİH:** 22.11.2025

Bu belge, sürüm yükseltmesinin teknik gerekçelerini, API değişikliklerini ve beklenen performans kazanımlarını belgeler.

## 1. Kritik Kazanımlar ve Düzeltmeler

Sürüm notları incelendiğinde, projemizdeki mevcut sorunları doğrudan hedefleyen şu kritik iyileştirmeler tespit edilmiştir:

### A. Halüsinasyon Giderme (Anti-Hallucination)
*   **Kaynak Sürüm:** v1.7.3
*   **Değişiklik:** "Fix hallucinations during silence" & "Implement no_speech_thold".
*   **Etki:** Mevcut sürümde sessiz anlarda modelin kendi kendine uydurduğu ("Altyazı...", "[Müzik]") gibi çıktıların motor seviyesinde engellenmesi.
*   **Eylem:** `whisper_full_params` yapısındaki `no_speech_thold` parametresinin `SttEngine` sınıfına entegre edilmesi.

### B. Yerleşik VAD (Voice Activity Detection)
*   **Kaynak Sürüm:** v1.7.6 & v1.8.1
*   **Değişiklik:** "Add initial VAD support" & "Fix memory leaks in VAD".
*   **Etki:** Şu an Omni-Studio (Frontend) tarafında yapılan basit RMS (ses şiddeti) tabanlı VAD yerine, Whisper'ın kendi eğitilmiş VAD yeteneğinin kullanılması. Bu, SIP gibi gürültülü kanallarda konuşma/sessizlik ayrımını çok daha hassas yapacaktır.

### C. Performans (Flash Attention)
*   **Kaynak Sürüm:** v1.8.0
*   **Değişiklik:** "Flash attention is now enabled by default".
*   **Etki:** Özellikle GPU (CUDA/Metal) kullanımlarında bellek bant genişliğinin daha verimli kullanılması ve token üretim hızının (RTF) artması. Uzun bağlamlarda (long context) bellek tüketimini azaltır.

---

## 2. API ve Parametre Değişiklikleri

`whisper.h` başlık dosyasında yapılan ve kodumuzu etkileyebilecek değişiklikler:

### 2.1. Parametre İsimlendirmeleri
*   `suppress_non_speech_tokens` parametresi `suppress_nst` olarak kısaltılmış veya alias eklenmiş olabilir (v1.7.4 notlarına istinaden). Kodda `whisper_full_params` yapısı kontrol edilmeli.

### 2.2. Yeni Parametreler
*   `float no_speech_thold`: Konuşma olmama olasılığı eşiği (Varsayılan: 0.6). Bu değerin üzerinde bir olasılıkla "sessizlik" tespit edilirse, transkripsiyon atlanır.
*   `bool flash_attn`: Flash attention kullanımı (Varsayılan: true).

---

## 3. Migrasyon Planı

### Adım 1: Dockerfile Güncellemesi
`Dockerfile` ve `Dockerfile.gpu` içindeki `WHISPER_CPP_VERSION` argümanı güncellenmelidir.

```dockerfile
# Eski
ARG WHISPER_CPP_VERSION=v1.7.1

# Yeni (Hedef)
ARG WHISPER_CPP_VERSION=v1.8.2
```

### Adım 2: CMake Yapılandırması
`v1.8.0` sonrası build sisteminde bazı değişiklikler raporlanmıştır. `CMakeLists.txt` içinde:
*   `GGML_CUDA` yerine `WHISPER_CUDA` bayrağı gerekip gerekmediği kontrol edilmelidir (Genelde geriye dönük uyumluluk vardır ama kontrol şart).
*   `flash-attn` desteği için CUDA mimarisi (Compute Capability) gereksinimleri doğrulanmalı (RTX 3060 destekler, sorun yok).

### Adım 3: SttEngine Refactoring
`src/stt_engine.cpp` dosyasında:
1.  `whisper_full_params` yapılandırmasına `wparams.no_speech_thold = settings_.no_speech_threshold;` atamasının doğruluğu teyit edilmeli.
2.  Eğer native VAD kullanılacaksa, `whisper_decode` öncesi VAD kontrolü mekanizması incelenmeli (Şimdilik transkripsiyon sırasındaki VAD yeterli).

---

## 4. Riskler ve Önlemler

| Risk | Önlem |
| :--- | :--- |
| **CUDA Sürüm Uyumsuzluğu** | `v1.8.x` daha yeni CUDA toolkit isteyebilir. Docker base imajımız `12.4.1`, bu yeterince güncel. |
| **Bellek Sızıntısı (VAD)** | `v1.8.1` sürümünde VAD bellek sızıntılarının düzeltildiği belirtilmiş. `v1.8.2` kullanacağımız için güvendeyiz. |
| **Model Uyumluluğu** | GGML/GGUF formatında bir değişiklik oldu mu? `sync: ggml` notları var. Mevcut modeller (`ggml-base.bin`) muhtemelen çalışır ancak gerekirse yeniden indirme (`download_models.sh`) gerekebilir.

---

## 5. Sonuç

`v1.8.2` sürümüne geçiş, projenin kararlılığı ve kalitesi (özellikle SIP entegrasyonu) için **KRİTİK** önem taşımaktadır. Performans artışı ve halüsinasyon giderme, kullanıcı deneyimini doğrudan iyileştirecektir.

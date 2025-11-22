# 🏛️ Mimari Karar Kayıtları (ADR) - Migrasyon v1.8.2

Bu belge, `whisper.cpp` v1.8.2 sürümüne geçiş sırasında yapılan teknik araştırmaların sonuçlarını ve alınan mimari kararları belgeler.

## ADR-001: Model Formatı Stratejisi (.bin vs .gguf)

*   **Durum:** Whisper.cpp ekosistemi GGUF formatına doğru kaymaktadır ancak elimizdeki modeller `.bin` formatındadır.
*   **Araştırma Sonucu:** v1.8.2 sürümü geriye dönük uyumluluk kapsamında `.bin` (GGML) formatını desteklemeye devam etmektedir. GGUF dönüşümü için henüz resmi ve stabil bir araç setine tam entegrasyon sağlanmamıştır.
*   **Karar:** **Mevcut `.bin` formatında kalınacaktır.**
*   **Gerekçe:** Production ortamında `Invalid Magic Number` riskini almamak ve mevcut indirme altyapısını (`download_models.sh`) bozmamak için stabilite önceliklendirilmiştir.

## ADR-002: VAD (Voice Activity Detection) Entegrasyonu

*   **Durum:** "Sessizlikte Halüsinasyon" (örn: `[Music]`, `Altyazı...`) sorunu yaşanmaktadır. v1.8.2 yerleşik VAD desteği sunmaktadır.
*   **Araştırma Sonucu:** Whisper.cpp'nin VAD özelliği, ana modelin içinde DEĞİLDİR. Harici bir `silero-vad` model dosyasına ihtiyaç duyar. Bu dosya sağlanmazsa `Segmentation Fault` riski vardır.
*   **Karar:** **Harici `ggml-silero-vad.bin` modeli zorunlu kılınmıştır.**
*   **Uygulama:**
    1.  `scripts/download_models.sh` güncellenerek bu modelin indirilmesi otomatiğe bağlandı.
    2.  `src/config.h` içine `vad_model_filename` parametresi eklendi.

## ADR-003: GPU Derleme Parametreleri

*   **Durum:** CMake parametreleri sürümler arasında değişkenlik göstermektedir (`WHISPER_CUBLAS`, `GGML_CUDA` vb.).
*   **Araştırma Sonucu:** v1.8.2 ve güncel `llama.cpp` çekirdeği için geçerli ve önerilen bayrak `-DGGML_CUDA=1` dir. Yanlış bayrak kullanımı GPU'nun devre dışı kalmasına ve performansın 10x düşmesine neden olur.
*   **Karar:** **`CMakeLists.txt` ve `Dockerfile.gpu` içinde `-DGGML_CUDA=1` kullanılacaktır.**
# 🏛️ Mimari Karar Kayıtları (ADR) - Migrasyon v1.8.2 & v2.2.0

Bu belge, `whisper.cpp` v1.8.2 sürümüne geçiş ve v2.2.0 geliştirmeleri sırasında alınan mimari kararları belgeler.

## ADR-001: Model Formatı Stratejisi (.bin vs .gguf)

*   **Durum:** Whisper.cpp ekosistemi GGUF formatına doğru kaymaktadır ancak elimizdeki modeller `.bin` formatındadır.
*   **Araştırma Sonucu:** v1.8.2 sürümü geriye dönük uyumluluk kapsamında `.bin` (GGML) formatını desteklemeye devam etmektedir. GGUF dönüşümü için henüz resmi ve stabil bir araç setine tam entegrasyon sağlanmamıştır.
*   **Karar:** **Mevcut `.bin` formatında kalınacaktır.**
*   **Gerekçe:** Production ortamında `Invalid Magic Number` riskini almamak ve mevcut indirme altyapısını bozmamak için stabilite önceliklendirilmiştir.

## ADR-002: VAD (Voice Activity Detection) Entegrasyonu

*   **Durum:** "Sessizlikte Halüsinasyon" (örn: `[Music]`, `Altyazı...`) sorunu yaşanmaktadır. v1.8.2 yerleşik VAD desteği sunmaktadır.
*   **Araştırma Sonucu:** Whisper.cpp'nin VAD özelliği, ana modelin içinde DEĞİLDİR. Harici bir `silero-vad` model dosyasına ihtiyaç duyar.
*   **Karar:** **Harici `ggml-silero-vad.bin` modeli zorunlu kılınmıştır.**
*   **Uygulama (GÜNCELLEME - v2.2.0):** Başlangıçta `scripts/download_models.sh` kullanılması planlanmıştı. Ancak **ADR-004** kararı ile bu işlem native C++ koduna (`ModelManager`) taşınmıştır. Script kullanımı iptal edilmiştir.

## ADR-003: GPU Derleme Parametreleri

*   **Durum:** CMake parametreleri sürümler arasında değişkenlik göstermektedir (`WHISPER_CUBLAS`, `GGML_CUDA` vb.).
*   **Araştırma Sonucu:** v1.8.2 ve güncel `llama.cpp` çekirdeği için geçerli ve önerilen bayrak `-DGGML_CUDA=1` dir.
*   **Karar:** **`CMakeLists.txt` ve `Dockerfile.gpu` içinde `-DGGML_CUDA=1` kullanılacaktır.** CPU buildleri için bu bayrak `0` yapılmalıdır.

## ADR-004: Native Auto-Provisioning (Script Bağımsızlığı)

*   **Durum:** Model dosyalarının (Whisper & VAD) indirilmesi için harici bir Bash scriptine (`download_models.sh`) güvenilmekteydi. Bu durum, konteyner taşınabilirliğini zorlaştırıyor ve hata yönetimini (Error Handling) işletim sistemi seviyesine bırakıyordu.
*   **Karar:** **Bash scripti tamamen kaldırılarak, indirme ve doğrulama mantığı `src/model_manager.cpp` içerisine (C++ Runtime) taşınmıştır.**
*   **Avantajlar:**
    1.  **Self-Contained:** Servis, çalışmak için sadece binary dosyasına ihtiyaç duyar. Script bağımlılığı yoktur.
    2.  **Sağlamlık:** İndirilen dosyanın boyutu ve bütünlüğü C++ içinde kontrol edilir, hatalıysa (örn: 404 sayfası indiyse) otomatik silinip tekrar denenir.
    3.  **Yönetim:** Model URL'leri ve dosya adları `config.h` üzerinden merkezi olarak yönetilir.
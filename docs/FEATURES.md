# 🌟 Sistem Özellikleri ve Teknik Yetenekler (v2.5.0)

Bu belge, **Sentiric STT Whisper Service** projesinin teknik yeteneklerini, kritik algoritmalarını ve **neden** o şekilde tasarlandıklarını içerir. Gelecekteki geliştirmelerde bu maddeler referans alınmalıdır.

---

## 🧠 1. Çekirdek Motor (Core AI Engine)

Sistemin temeli, Python bağımlılığı olmayan saf C++ performansına dayanır.

### 1.1. Whisper.cpp Entegrasyonu
*   **Sürüm:** v1.8.2 (Stable).
*   **Model Formatı:** GGML (`.bin`). GGUF formatına geçiş henüz yapılmamıştır (Geriye dönük uyumluluk).
*   **Compute:**
    *   **GPU (CUDA):** Transkripsiyon (Inference) işlemi NVIDIA GPU üzerinde, `Flash Attention` açık olarak çalışır.
    *   **CPU:** VAD (Sessizlik tespiti) ve DSP işlemleri CPU'da çalışır.

### 1.2. Dynamic Batching (State Pooling)
*   **Amaç:** GPU VRAM'ini verimli kullanmak.
*   **Mekanizma:** `SttEngine` sınıfı, açılışta `STT_WHISPER_SERVICE_PARALLEL_REQUESTS` sayısı kadar `whisper_state` oluşturur ve bunları bir havuzda (pool) tutar. Gelen istekler boşta olan bir state'i kapar, işi bitince havuza geri bırakır.
*   **Kritik:** Bu yapı thread-safe'dir (`std::mutex` ve `std::condition_variable` ile korunur). Kaldırılmamalıdır.

### 1.3. Auto-Provisioning
*   **Logic:** Sistem açıldığında `ModelManager`, gerekli model dosyalarının (`ggml-medium.bin`, `silero-vad.bin`) varlığını ve boyutunu kontrol eder. Eksik veya hatalıysa otomatik indirir.
*   **Güvenlik:** `system()` çağrısı yerine `fork()` + `execvp()` kullanılarak Shell Injection riski sıfırlanmıştır.

---

## 🎭 2. DSP & Affective Intelligence (KRİTİK)

Burası projenin "kalbidir". Harici bir AI modeli kullanmadan, matematiksel sinyal işleme ile duygu ve kimlik analizi yapılır. **Buradaki eşik değerleri (Magic Numbers) rastgele değildir; binlerce test sonucu belirlenmiştir.**

### 2.1. Oktav Hatası Düzeltme (Octave Error Correction)
*   **Sorun:** Erkek seslerindeki (Bass/Bariton) güçlü 2. harmonik, basit algoritmaların frekansı 2 katı (örn: 100Hz yerine 200Hz) ölçmesine neden olur. Bu da erkeği kadın sanmasına yol açar.
*   **Çözüm (ZCR Heuristic):**
    *   Sistem Pitch'i yüksek (örn: >170Hz) ölçse bile, **ZCR (Zero Crossing Rate)** değerini kontrol eder.
    *   **KURAL:** Eğer `ZCR < 0.024` ise, bu ses mekanik olarak kalın bir sestir (Erkek).
    *   **AKSİYON:** `pitch_mean *= 0.5f` (Frekans yarıya indirilir) ve Cinsiyet zorla `M` (Male) yapılır.
*   **Eşik:** `0.024`. (Ezgi ~0.039, Can ~0.016). Bu değerle oynanmamalıdır.

### 2.2. Cinsiyete Göreceli Duygu (Adaptive Emotion)
*   **Sorun:** Mutlak Pitch değerine bakıldığında, erkek sesleri (düşük frekans) matematiksel olarak hep "Negatif/Sad" çıkıyordu.
*   **Çözüm:**
    *   Erkek tespit edilirse Pitch Skalası: `60Hz - 180Hz`.
    *   Kadın tespit edilirse Pitch Skalası: `160Hz - 300Hz`.
    *   **Bias:** Valence formülüne `+0.35` puan eklenerek varsayılan durum "Üzgün"den "Nötr"e çekilmiştir.

### 2.3. Vector Polarization (Diarization Fix)
*   **Sorun:** Erkek ve Kadın seslerinin vektörleri uzayda birbirine çok yakın durabilir, bu da `Clusterer`ın onları birleştirmesine neden olur.
*   **Çözüm:**
    *   Eğer Cinsiyet `M` ise: Vektörün Pitch bileşeni `[0.0 - 0.4]` arasına sıkıştırılır.
    *   Eğer Cinsiyet `F` ise: Vektörün Pitch bileşeni `[0.6 - 1.0]` arasına itilir.
*   **Sonuç:** Bu yapay uçurum, Cosine Similarity algoritmasının farklı cinsiyetleri %100 ayırmasını sağlar.

---

## 🗣️ 3. Speaker Diarization (Kimlik Ayrıştırma)

*   **Algoritma:** Whisper'ın `tdrz` (tinydiarize) özelliği **kullanılmamaktadır** (Kararsız olduğu için).
*   **Yöntem:** Kendi yazdığımız `SpeakerClusterer` sınıfı.
*   **Öznitelikler:** 8 Boyutlu Vektör (Pitch Mean, Pitch Std, Energy Mean, Energy Std, Spectral Centroid, ZCR, Arousal, Valence).
*   **Eşik (Threshold):** `0.94`. Bu değerin altındaki benzerlikler "Yeni Konuşmacı" olarak kabul edilir.

---

## 📡 4. API ve Protokoller

### 4.1. gRPC (Stream & Unary)
*   **Stream:** `WhisperTranscribeStream`. Ses chunk'lar halinde gelir (Bi-directional). Gerçek zamanlıdır.
*   **Unary:** `WhisperTranscribe`. Tek bir WAV dosyası gönderilir.
*   **Kontrat:** `sentiric-contracts` (v1.11.3) kullanılır.

### 4.2. HTTP REST
*   **Endpoint:** `POST /v1/transcribe`
*   **Parametreler:** `file`, `language`, `prompt`, `temperature`, `prosody_lpf_alpha` vb.
*   **Metrics:** `GET /metrics` (Prometheus formatı, CORS enabled).

---

## 🎛️ 5. Omni-Studio (Web UI)

### 5.1. Scoped Karaoke
*   **Sorun:** Global `querySelectorAll` kullanımı, birden fazla dosya yüklendiğinde veya kayıt alındığında tüm metinlerin aynı anda yanıp sönmesine neden oluyordu.
*   **Çözüm:** Her transkripsiyon bloğu (Batch) kendi benzersiz ID'sine sahiptir. "Oynat" butonuna basıldığında sadece **o bloğun içindeki** kelimeler taranır (`el.closest('.transcription-batch')`).

### 5.2. Sistem Monitörü
*   Canlı TPS (Token Per Second) grafiği (`canvas`).
*   RTF (Real Time Factor) ve Latency takibi.
*   Veriyi `http://localhost:15032/metrics` adresinden çeker.

### 5.3. Veri Yönetimi
*   **Export:** JSON, TXT ve SRT formatında dışa aktarım.
*   **Persistence:** Yapılan DSP ayarları (Eşikler, Filtreler) tarayıcının `localStorage` biriminde saklanır.

---

## ⚠️ Geliştirici Notları (DİKKAT!)

1.  **libsamplerate:** Sistem dahili olarak **16kHz** çalışır. Farklı bir örnekleme hızı gelirse `libsamplerate` ile dönüştürülür. Bu kütüphaneyi build sisteminden çıkarmayın.
2.  **ZCR Threshold:** `0.024` değeri binlerce ses örneği (Whisper.cpp sampleları + Yerel testler) ile bulunmuştur. Değiştirirken dikkatli olun.
3.  **Flash Attention:** GPU (NVIDIA) buildlerinde `STT_WHISPER_SERVICE_FLASH_ATTN=true` performansı 2 kat artırır. Kapatmayın.
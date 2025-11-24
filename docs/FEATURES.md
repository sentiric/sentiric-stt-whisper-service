# 🌟 Sistem Özellikleri ve Teknik Yetenekler

Bu belge, **Sentiric STT Whisper Service (v2.5.0 - Omni-Studio v7)** platformunun sunduğu tüm teknik yetenekleri, sinyal işleme algoritmalarını ve kullanıcı arayüzü özelliklerini detaylandırır.

---

## 🧠 1. Çekirdek Motor (Core AI Engine)

Sistemin kalbi, Python bağımlılığı olmayan, saf C++ performansı üzerine kuruludur.

*   **Native C++ Mimarisi:** `whisper.cpp` v1.8.2 çekirdeği ile Python GIL (Global Interpreter Lock) darboğazı olmadan çalışır.
*   **Hibrit Hesaplama (Hybrid Compute):**
    *   **VAD (Sessizlik Tespiti):** Silero VAD (v5), CPU üzerinde çalışarak GPU kaynaklarını boşa harcamaz.
    *   **Inference (Transkripsiyon):** NVIDIA CUDA ve `Flash Attention` optimizasyonu ile GPU üzerinde ultra hızlı çıkarım yapar.
*   **Dynamic Batching:** "State Pooling" mimarisi sayesinde, aynı anda gelen çoklu istekleri (Parallel Requests) GPU belleğinde birleştirerek işler.
*   **Auto-Provisioning:** Model dosyaları (GGML/GGUF) ve VAD modelleri, konteyner ilk açılışında otomatik olarak doğrulanır ve indirilir.

---

## 🎭 2. Duyuşsal Zeka ve DSP (Affective Intelligence)

Sadece metni değil, **sesin "nasıl" söylendiğini** analiz eden, ek model yükü getirmeyen (Zero-Latency) sinyal işleme katmanı.

### 2.1. Prosody & Feature Extraction
*   **Advanced Pitch Tracking:** Center-Clipping ve Median Filtering yöntemleri ile gürültülü ortamlarda bile temel frekansı (F0) doğru tespit eder.
*   **Harmonic Correction:** Erkek seslerinde oluşan "Oktav Hatalarını" (2. harmoniğin yakalanması) önleyen heuristic algoritmalar.
*   **LPF (Low-Pass Filter):** Yüksek frekanslı dijital gürültüyü temizleyen, ayarlanabilir `Alpha` katsayılı filtreleme.
*   **Spectral Centroid:** Sesin "parlaklığını" ve tınısını (Timbre) analiz eder.

### 2.2. Duygu ve Kimlik (Proxies)
*   **Cinsiyet Tahmini:** Pitch ve Spectral özelliklere dayalı, parametrik eşik değerli (örn: 170Hz) anlık cinsiyet tahmini.
*   **Duygu Haritalama:** Arousal (Uyarılma) ve Valence (Hoşnutluk) uzayında sesin enerjisine göre "Excited", "Sad", "Neutral", "Angry" etiketlemesi.
*   **Speaker Vector (8-D):** Konuşmacının ses karakteristiğini temsil eden 8 boyutlu normalize edilmiş vektör çıktısı.

---

## 📡 3. API ve Entegrasyon

Esnek ve parametrik yapı sayesinde "Hard-Coding" engellenmiştir. Her istek kendi konfigürasyonuyla işlenebilir.

### 3.1. Protokoller
*   **gRPC (High Performance):** Canlı ses akışı (Bi-directional Streaming) ve tekil dosya gönderimi için Protobuf kontratları.
*   **HTTP REST:** Dosya yükleme ve basit entegrasyonlar için `/v1/transcribe` endpoint'i.
*   **Prometheus Metrics:** RTF (Real-Time Factor), Latency ve İşlenen Ses Süresi metriklerinin `/metrics` üzerinden sunumu.

### 3.2. Parametrik Kontrol (Per-Request Config)
İstemciler, her istekte şu ayarları dinamik olarak değiştirebilir:
*   `temperature` & `beam_size`: Modelin yaratıcılığı ve arama derinliği.
*   `prosody_lpf_alpha`: Gürültü engelleme filtresinin şiddeti.
*   `prosody_pitch_gate`: Cinsiyet ayrımı için frekans eşiği.
*   `enable_diarization`: Konuşmacı ayrıştırmayı aç/kapat.

---

## 🎛️ 4. Omni-Studio v7 (Web UI)

Sistemi test etmek, ince ayar yapmak ve veriyi görselleştirmek için geliştirilmiş "Workstation" arayüzü.

### 4.1. Kullanıcı Deneyimi (UX)
*   **Mobile-First Design:** Responsive Sidebar, Dock yapısı ve dokunmatik dostu kontroller ile mobilde tam performans.
*   **Glassmorphism UI:** Modern, koyu tema (Dark Mode) ve akışkan animasyonlar.
*   **Persistent Config:** Yapılan tüm ayarlar (Filtre gücü, API adresi, Tema) tarayıcıda (`localStorage`) saklanır.

### 4.2. Özellikler
*   **Canlı Transkript Akışı:** Konuşmacı değişimlerini, duyguyu ve metni gerçek zamanlı akan bir sohbet arayüzünde gösterir.
*   **Interactive Playback:** Her segmentin yanında, o cümleye ait ses kaydını çalan mini player ve **WAV İndirme** butonu.
*   **Hands-Free VAD:** Tarayıcı tabanlı ses aktivitesi tespiti ile butona basmadan otomatik kayıt ve gönderim.
*   **Visualizer:** Mikrofon girişini frekans spektrumu olarak çizen canlı Canvas görselleştirme.
*   **DSP Tuning Panel:** Filtre gücü, Pitch eşiği ve Kümeleme hassasiyetini arayüz üzerinden anlık değiştirme imkanı.
*   **Export:** Çıktıları `JSON` (veri analizi için) veya `TXT` (okuma için) formatında dışa aktarma.

---

## 📊 5. Performans Metrikleri

*   **RTF (Real-Time Factor):** Ses süresine göre işlemin ne kadar hızlı yapıldığı (örn: 30x = 30 saniyelik ses 1 saniyede işlendi).
*   **Confidence Score:** Modelin kelime bazlı güven skorları.
*   **Processing Time:** Ağ gecikmesi hariç saf işlem süresi.
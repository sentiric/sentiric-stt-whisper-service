# 📡 API Referansı ve Kontratlar

Bu belge, Sentiric STT Whisper Servisi'nin dış dünyaya sunduğu **gRPC** ve **HTTP (REST)** arayüzlerini tanımlar.

## 1. gRPC Servisi: `SttWhisperService`

Bu servis, yüksek performanslı ve düşük gecikmeli iletişim için ana arayüzdür. Kontratlar `sentiric-contracts` reposunda tanımlanmıştır.

### 1.1. Servis Tanımı
```protobuf
service SttWhisperService {
  // Tek bir ses dosyasını bütün olarak işler.
  rpc WhisperTranscribe(WhisperTranscribeRequest) returns (WhisperTranscribeResponse);
  
  // Canlı ses akışını işler (Streaming).
  rpc WhisperTranscribeStream(stream WhisperTranscribeStreamRequest) returns (stream WhisperTranscribeStreamResponse);
}
```

### 1.2. Mesaj Tipleri
**WhisperTranscribeRequest**
*   `bytes audio_data`: Ham ses verisi (WAV headerlı veya headersız PCM).
*   `string language`: (Opsiyonel) "tr", "en" vb.
    *   ℹ️ **Not:** Eğer bu alan dolu gönderilirse, sunucudaki `STT_WHISPER_SERVICE_LANGUAGE` ayarını **geçersiz kılar (override)** ve bu dili kullanır.

**WhisperTranscribeResponse**
*   `string transcription`: Üretilen metin.
*   `string language`: Algılanan dil.
*   `float language_probability`: Güven skoru.
*   `double duration`: Ses süresi (saniye).

---

## 2. HTTP REST API

Web istemcileri (Omni-Studio) ve basit entegrasyonlar için sunulan endpoint'ler.

### 2.1. Transkripsiyon (`POST /v1/transcribe`)
Ses dosyasını yükleyerek metin çıktısı alır.

*   **Content-Type:** `multipart/form-data`
*   **Parametre:** `file` (Binary ses dosyası - WAV önerilir)
*   **Örnek Yanıt:**
    ```json
    {
      "text": "Merhaba dünya.",
      "language": "tr",
      "duration": 2.5
    }
    ```
*   **Dil Seçimi:** Şu an için REST API her zaman `STT_WHISPER_SERVICE_LANGUAGE` (Env Var) değerini veya Otomatik Algılamayı kullanır.

### 2.2. Sağlık Kontrolü (`GET /health`)
Servisin ve modelin durumunu bildirir. Orchestrator (K8s) liveness probe için kullanılır.

*   **Başarılı (200 OK):**
    ```json
    {
      "status": "healthy",
      "model_ready": true,
      "service": "sentiric-stt-whisper-service",
      "version": "2.0.0"
    }
    ```
*   **Başarısız (503 Service Unavailable):** Model henüz yüklenmediyse veya hata varsa.

---

## 3. Teknik Sınırlamalar ve Standartlar

1.  **Ses Formatı:** Servis dahili olarak **16kHz** örnekleme hızı kullanır. Farklı formatlar (örn: 8kHz) otomatik olarak dönüştürülür (`libsamplerate` ile), ancak en iyi performans için 16kHz WAV önerilir.
2.  **Concurrency:** `STT_WHISPER_SERVICE_THREADS` ortam değişkeni ile CPU thread kullanımı sınırlanabilir. Varsayılan: 4.

# 📚 Dökümana Ekleme – "Ne Nedir?" Açıklamaları

Aşağıdaki **tam metinleri**  
`docs/API_REFERENCE.md` **sonuna** **kopyala-yapıştır** – **commit** ile **birlikte** gitsin.

---

## 🆕 9. Yeni Duygu & Konuşmacı Kimliği Alanları (v2.4.0)

Bu bölüm, **zero-latency** prosody analizi ile elde edilen **duygu**, **cinsiyet** ve **konuşmacı vektörü** alanlarını açıklar.  
**Hiçbir ek model** yüklenmez; **sadece whisper.cpp çıktısı** kullanılır.

### 9.1 Affective Proxies (Duygu & Cinsiyet)

| Alan | Tip | Birim | Açıklama |
|---|---|---|---|
| `gender_proxy` | `string` | - | **"M"** veya **"F"** – *pitch mean > 165 Hz → F* |
| `emotion_proxy` | `string` | - | **"excited"**, **"neutral"**, **"sad"**, **"angry"** <br> *arousal + valence kural tabanı* |
| `arousal` | `float` | 0-1 | **Enerji düzeyi** – *RMS energy × 20* |
| `valence` | `float` | -1..1 | **Pozitiflik** – *pitch mean’e göre* |

> **Not**: Bu değerler **proxy**’dir; **%100 doğruluk** garantisi **yoktur**, **UI** için **görsel ipucu** sağlar.

---

### 9.2 Prosodic Features (Pitch & Timbre)

| Alan | Tip | Birim | Açıklama |
|---|---|---|---|
| `pitch_mean` | `float` | Hz | Segmentin **ortalama temel frekansı** |
| `pitch_std` | `float` | Hz | **Pitch değişkenliği** (standart sapma) |
| `energy_mean` | `float` | RMS | **Ortalama ses şiddeti** |
| `energy_std` | `float` | RMS | **Enerji değişkenliği** |
| `spectral_centroid` | `float` | k | **Timbre parlaklığı** (kaba proxy) |
| `zero_crossing_rate` | `float` | 0-1 | **Sinyal "keskinliği"** (yüksek = tiz)**

---

### 9.3 Speaker Identity Vector

| Alan | Tip | Boyut | Açıklama |
|---|---|---|---|
| `speaker_vec` | `[]float` | **8** | **Pitch, Energy, Timbre** özelliklerinin **normalize** hali: <br> `[pitch/300, pitch_std/50, energy, energy_std, spectral/1000, zcr, arousal, (valence+1)/2]` |

> **Kullanım**:  
> - **Aynı vektör** → **aynı konuşmacı** (UI’da **aynı renk**)  
> - **Farklı vektör** → **yeni konuşmacı** (UI’da **yeni renk**)  
> - **Tıkla** → **isim ver** (localStorage saklanır)

---

### 9.4 Örnek JSON Parçası
```json
{
  "gender": "F",
  "emotion": "excited",
  "arousal": 0.82,
  "valence": 0.55,
  "pitch_mean": 210.3,
  "pitch_std": 18.4,
  "energy_mean": 0.08,
  "energy_std": 0.01,
  "spectral_centroid": 85.7,
  "zero_crossing_rate": 0.31,
  "speaker_vec": [0.71, 0.37, 0.08, 0.01, 0.086, 0.31, 0.82, 0.77]
}
```

---

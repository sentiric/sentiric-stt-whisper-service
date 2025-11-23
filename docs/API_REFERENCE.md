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

**Yeni alanlar (zero-latency rich prosody + speaker-vector):**
- `gender`           : "M" / "F"  (pitch mean)
- `emotion`          : "excited", "neutral", "sad", "angry"
- `arousal`          : 0.0 - 1.0  (energy)
- `valence`          : -1.0 - 1.0 (pitch+energy)
- `pitch_mean`       : Hz
- `pitch_std`        : Hz
- `energy_mean`      : RMS
- `energy_std`       : RMS
- `spectral_centroid`: timbre proxy
- `zero_crossing_rate`: brightness
- `speaker_vec`      : 8-D float vector → kümeleme için
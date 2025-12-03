# 📡 API Referansı ve Kontratlar

Bu belge, Sentiric STT Whisper Servisi'nin dış dünyaya sunduğu **gRPC** ve **HTTP (REST)** arayüzlerini tanımlar.

## 1. gRPC Servisi: `SttWhisperService` (Dahili / Yüksek Performans)

Bu servis, Sentiric ekosistemi içindeki diğer servisler (Gateway, Agent) ile haberleşmek için kullanılır. Kontratlar `sentiric-contracts` reposunda tanımlanmıştır.

### 1.1. Servis Tanımı
```protobuf
service SttWhisperService {
  // Tek bir ses dosyasını bütün olarak işler.
  rpc WhisperTranscribe(WhisperTranscribeRequest) returns (WhisperTranscribeResponse);
  
  // Canlı ses akışını işler (Streaming).
  rpc WhisperTranscribeStream(stream WhisperTranscribeStreamRequest) returns (stream WhisperTranscribeStreamResponse);
}
```

---

## 2. HTTP REST API (Harici / Standalone Kullanım)

Web istemcileri, mobil uygulamalar ve 3. parti entegrasyonlar için sunulan standart REST arayüzü.

### 2.1. Transkripsiyon (`POST /v1/transcribe`)

Ses dosyasını yükleyerek zenginleştirilmiş metin ve analiz çıktısı alır.

*   **URL:** `http://localhost:15030/v1/transcribe`
*   **Method:** `POST`
*   **Content-Type:** `multipart/form-data`

#### **Parametreler (Form Data)**

| Parametre | Tip | Zorunlu | Varsayılan | Açıklama |
|---|---|---|---|---|
| `file` | File | **Evet** | - | İşlenecek ses dosyası (WAV, MP3, WebM desteklenir). |
| `language` | String | Hayır | `auto` | Kaynak dil kodu (örn: `tr`, `en`). |
| `prompt` | String | Hayır | - | Modele bağlam (context) veya stil ipucu vermek için metin. |
| `diarization` | Bool | Hayır | `true` | Konuşmacı ayrıştırmayı etkinleştir (`true`/`false`). |
| `temperature` | Float | Hayır | `0.0` | Modelin "yaratıcılığı". Düşük değerler daha deterministiktir. |
| `prosody_pitch_gate` | Int | Hayır | `170` | Cinsiyet ayrımı için frekans eşiği (Hz). |

#### **Başarılı Yanıt (200 OK)**

```json
{
  "text": "Merhaba, Sentiric platformuna hoş geldiniz.",
  "language": "tr",
  "duration": 3.45,
  "segments": [
    {
      "text": "Merhaba, Sentiric platformuna hoş geldiniz.",
      "start": 0.0,
      "end": 3.45,
      "probability": 0.98,
      "speaker_id": "spk_0",
      "speaker_turn_next": false,
      
      // --- Duyuşsal Analiz (Affective Intelligence) ---
      "gender": "F",           // Tahmini Cinsiyet (F/M)
      "emotion": "neutral",    // Tahmini Duygu
      "arousal": 0.45,         // Enerji Seviyesi (0.0 - 1.0)
      "valence": 0.10,         // Pozitiflik Seviyesi (-1.0 - 1.0)
      "pitch_mean": 215.4,     // Ortalama Ses Frekansı (Hz)
      "pitch_std": 12.1,       // Frekans Değişkenliği
      
      // --- Kelime Detayları ---
      "words": [
        { "word": "Merhaba", "start": 0.0, "end": 0.8, "probability": 0.99 },
        { "word": "Sentiric", "start": 0.9, "end": 1.5, "probability": 0.95 },
        // ...
      ]
    }
  ],
  "meta": {
    "processing_time": 0.42, // Saniye cinsinden işlem süresi
    "rtf": 8.2,              // Real-Time Factor (Hız katsayısı)
    "tokens": 12             // Üretilen token sayısı
  }
}
```

#### **Hata Yanıtı (4xx/5xx)**

```json
{
  "error": "Model not ready" // veya "Invalid audio format"
}
```

### 2.2. Sağlık Kontrolü (`GET /health`)
Servisin ve modelin durumunu bildirir. Yük dengeleyiciler ve Kubernetes liveness probe'ları için kullanılır.

```json
{
  "status": "healthy",
  "model_ready": true,
  "service": "sentiric-stt-whisper-service",
  "version": "2.5.1",
  "api_compatibility": "openai-whisper"
}
```

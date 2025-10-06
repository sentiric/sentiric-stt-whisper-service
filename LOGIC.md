# 🤫 Sentiric STT Whisper Service - Mantık ve Akış Mimarisi

**Stratejik Rol:** Yüksek performanslı ve yerel (on-premise/GPU) ortamlara optimize edilmiş, saf Konuşma Tanıma yeteneğini sunar. Sadece STT Gateway'den gelen ham ses verisini işleyip metin döndürmekten sorumludur.

---

## 1. Temel Akış: Transkripsiyon (WhisperTranscribe)

```mermaid
sequenceDiagram
    participant Gateway as STT Gateway
    participant Whisper as Whisper Service
    
    Gateway->>Whisper: WhisperTranscribe(audio_data, language)
    
    Note over Whisper: 1. Ses Ön İşleme (16kHz, mono)
    Note over Whisper: 2. Model Çalıştırma (GPU/CPU)
    Whisper-->>Gateway: WhisperTranscribeResponse(transcription)
```

## 2. Optimizasyon
* Model Caching: Model dosyaları (large-v3, medium) Docker volume'ler aracılığıyla kalıcı olarak saklanmalıdır (Hugging Face cache).
* Donanım Kullanımı: CUDA/GPU desteği öncelikli olmalıdır, ancak WHISPER_DEVICE ayarı ile CPU'da da çalışabilir.
* Tek Modellik Odak: Bu servis yalnızca Whisper'a odaklanır. Protokol normalleştirme veya karmaşık yönlendirme yapmaz.
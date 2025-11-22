# 🏗️ Sistem Mimarisi (v2.0)

Sentiric STT Whisper Service, yüksek performanslı ses işleme ve transkripsiyon için tasarlanmış, C++ tabanlı bir mikroservistir.

## 1. Mimari Bileşenler

```mermaid
graph TD
    Client[Client (Gateway/Agent)] -- gRPC Audio Stream --> gRPC_Server
    Client -- HTTP REST (File) --> HTTP_Server
    
    subgraph "STT Service Container"
        gRPC_Server[gRPC Server]
        HTTP_Server[HTTP Server]
        
        subgraph "Core Engine"
            Resampler[LibSamplerate (8kHz -> 16kHz)]
            WhisperEngine[Whisper.cpp Engine]
            ModelMgr[Model Manager]
        end
        
        ModelFiles[(GGUF Models)]
    end
    
    gRPC_Server --> Resampler
    HTTP_Server --> Resampler
    Resampler --> WhisperEngine
    ModelMgr -- Loads --> WhisperEngine
    WhisperEngine -- Reads --> ModelFiles
```

## 2. Akış Mantığı

1.  **Girdi:** İstemci, gRPC (Stream) veya HTTP (POST) üzerinden ses verisi gönderir. Ses formatı genellikle 8kHz (Telephony) veya 16kHz'dir.
2.  **Ön İşleme (Preprocessing):** `stt_engine`, gelen sesi analiz eder. Eğer örnekleme hızı 16kHz değilse, `libsamplerate` kullanarak yüksek kaliteli dönüşüm yapar. Ayrıca 16-bit INT verisini 32-bit FLOAT formatına normalize eder.
3.  **Çıkarım (Inference):** `whisper.cpp`, ses verisini işler. Konfigürasyona göre `Beam Search` veya `Greedy` stratejisi kullanır. VAD (Voice Activity Detection), sessiz bölümleri filtreler.
4.  **Çıktı:** Metin (Transcript), Dil ve Zaman Damgası bilgileri istemciye döner.

## 3. Teknik Standartlar
*   **Dil:** C++17
*   **Concurrency:** Native Threading (Python GIL yok).
*   **Build:** CMake + vcpkg + Docker Multi-stage.
*   **Model Formatı:** GGML/GGUF (Whisper.cpp uyumlu).


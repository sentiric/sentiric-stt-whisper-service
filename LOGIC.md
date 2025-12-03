## 🏗️ Mimari

```mermaid
graph TD
    Input[Audio Input] --> Resampler[Resampler 16kHz]
    Resampler --> VAD[Silero VAD (CPU)]
    VAD --> Queue[Request Queue (Timeout Protected)]
    Queue --> Whisper[Whisper Encoder (GPU)]
    
    subgraph "DSP & Affective Engine"
        PCM[PCM Data] --> Pitch[Pitch/ZCR Analysis]
        Pitch --> Correction[Octave Error Correction]
        Correction --> Gender[Gender Classification]
        Gender --> Emotion[Relative Emotion Mapping]
        Gender --> Vector[Vector Polarization]
    end
    
    Whisper --> Tokens[Text Tokens]
    Tokens --> JSON[Final JSON Response]
    Vector --> JSON
    Emotion --> JSON
```
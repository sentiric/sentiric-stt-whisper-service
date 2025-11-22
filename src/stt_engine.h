#pragma once

#include "config.h"
#include "whisper.h" // whisper.cpp v1.8.2
#include <vector>
#include <string>
#include <mutex>
#include <memory>

struct TokenData {
    std::string text;
    float p;       
    int64_t t0;    
    int64_t t1;    
};

struct TranscriptionResult {
    std::string text;
    std::string language;
    float prob;
    int64_t t0; 
    int64_t t1; 
    std::vector<TokenData> tokens; 
};

class SttEngine {
public:
    explicit SttEngine(const Settings& settings);
    ~SttEngine();

    bool is_ready() const;

    std::vector<TranscriptionResult> transcribe(
        const std::vector<float>& pcmf32, 
        int input_sample_rate = 16000,
        const std::string& language = "" 
    );

    std::vector<TranscriptionResult> transcribe_pcm16(
        const std::vector<int16_t>& pcm16, 
        int input_sample_rate = 16000,
        const std::string& language = ""
    );

private:
    std::vector<float> resample_audio(const std::vector<float>& input, int src_rate, int target_rate);
    
    // VAD (Voice Activity Detection) Kontrolü
    bool is_speech_detected(const std::vector<float>& pcmf32);

    Settings settings_;
    struct whisper_context* ctx_ = nullptr;
    
    // YENİ: VAD Context (Silero)
    // Whisper.cpp v1.8.2 vad struct'ı değiştiği için generic void* veya doğru struct kullanılmalı.
    // Header'da forward declaration sorunu yaşamamak için pointer tutuyoruz.
    struct whisper_vad_context* vad_ctx_ = nullptr; 
    
    std::mutex mutex_; 
};
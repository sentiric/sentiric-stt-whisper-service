#pragma once

#include "config.h"
#include "whisper.h"
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <queue>
#include <condition_variable>

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
    bool is_speech_detected(const std::vector<float>& pcmf32);

    // Yardımcı: Havuzdan boş bir state al
    struct whisper_state* acquire_state();
    // Yardımcı: State'i havuza geri bırak
    void release_state(struct whisper_state* state);

    Settings settings_;
    struct whisper_context* ctx_ = nullptr; // Ana Model (Shared Read-Only)
    struct whisper_vad_context* vad_ctx_ = nullptr;
    
    // --- Dynamic Batching: State Pool ---
    std::queue<struct whisper_state*> state_pool_; // Boştaki state'ler
    std::mutex pool_mutex_;
    std::condition_variable pool_cv_;
    std::vector<struct whisper_state*> all_states_; // Temizlik için tüm state referansları

    std::mutex vad_mutex_; // VAD thread-safe olmayabilir, koruyoruz.
};
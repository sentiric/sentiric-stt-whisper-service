#pragma once

#include "config.h"
#include "whisper.h"
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <queue>
#include <condition_variable>

// Token verisi
struct TokenData {
    std::string text;
    float p;       
    int64_t t0;    
    int64_t t1;    
};

// İstek bazında özelleştirilebilir parametreler (Pro Özellikler)
struct RequestOptions {
    std::string language = "";      // Boş ise auto-detect
    std::string prompt = "";        // Context/İpucu
    bool translate = false;         // X -> English çeviri
    bool enable_diarization = false;// Konuşmacı ayrıştırma
    
    // Gelişmiş (Advanced)
    float temperature = -1.0f;      // -1 ise varsayılanı kullan
    int beam_size = -1;             // -1 ise varsayılanı kullan
    int best_of = -1;               // -1 ise varsayılanı kullan
};

struct TranscriptionResult {
    std::string text;
    std::string language;
    float prob;
    int64_t t0; 
    int64_t t1; 
    bool speaker_turn_next;
    std::vector<TokenData> tokens; 
};

class SttEngine {
public:
    explicit SttEngine(const Settings& settings);
    ~SttEngine();

    bool is_ready() const;

    // Yeni İmza: Parametreler struct içinde toplanmıştır
    std::vector<TranscriptionResult> transcribe(
        const std::vector<float>& pcmf32, 
        int input_sample_rate,
        const RequestOptions& options
    );

    std::vector<TranscriptionResult> transcribe_pcm16(
        const std::vector<int16_t>& pcm16, 
        int input_sample_rate,
        const RequestOptions& options
    );

private:
    std::vector<float> resample_audio(const std::vector<float>& input, int src_rate, int target_rate);
    bool is_speech_detected(const std::vector<float>& pcmf32);

    struct whisper_state* acquire_state();
    void release_state(struct whisper_state* state);

    Settings settings_;
    struct whisper_context* ctx_ = nullptr;
    struct whisper_vad_context* vad_ctx_ = nullptr;
    
    std::queue<struct whisper_state*> state_pool_; 
    std::mutex pool_mutex_;
    std::condition_variable pool_cv_;
    std::vector<struct whisper_state*> all_states_; 

    std::mutex vad_mutex_;
};
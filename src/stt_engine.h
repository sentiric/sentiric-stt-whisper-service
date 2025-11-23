#pragma once
#include "config.h"
#include "whisper.h"
#include "prosody_extractor.h"   // <-- yeni
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

struct RequestOptions {
    std::string language;
    std::string prompt;
    bool translate = false;
    bool enable_diarization = false;
    float temperature = -1.0f;
    int beam_size = -1;
    int best_of = -1;
};

struct TranscriptionResult {
    std::string text;
    std::string language;
    float prob;
    int64_t t0;
    int64_t t1;
    bool speaker_turn_next;
    std::vector<TokenData> tokens;
    // ---- zero-cost affective proxies ----
    std::string gender_proxy;
    std::string emotion_proxy;
    float arousal = 0.0f;
    float valence = 0.0f;
    AffectiveTags affective; // tüm feature'lar
};

class SttEngine {
public:
    explicit SttEngine(const Settings& settings);
    ~SttEngine();
    bool is_ready() const;
    std::vector<TranscriptionResult> transcribe(const std::vector<float>& pcmf32, int input_sample_rate, const RequestOptions& options);
    std::vector<TranscriptionResult> transcribe_pcm16(const std::vector<int16_t>& pcm16, int input_sample_rate, const RequestOptions& options);
private:
    std::vector<float> resample_audio(const float* input, size_t input_size, int src_rate, int target_rate);
    bool is_speech_detected(const float* pcm, size_t n_samples);
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
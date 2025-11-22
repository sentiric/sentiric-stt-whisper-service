#include "stt_engine.h"
#include "spdlog/spdlog.h"
#include <samplerate.h>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <numeric>

SttEngine::SttEngine(const Settings& settings) : settings_(settings) {
    std::string model_path = settings_.model_dir + "/" + settings_.model_filename;
    spdlog::info("Loading Whisper model from: {}", model_path);

    struct whisper_context_params cparams = whisper_context_default_params();
    #ifdef GGML_USE_CUDA
    spdlog::info("CUDA detected. Enabling GPU offloading for Main Engine.");
    cparams.use_gpu = true;
    if (settings_.flash_attn) cparams.flash_attn = true; 
    #endif

    ctx_ = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    if (!ctx_) throw std::runtime_error("Whisper model initialization failed");

    int pool_size = settings_.parallel_requests;
    if (pool_size < 1) pool_size = 1;
    spdlog::info("⚡ Initializing Dynamic Batching Pool with {} states...", pool_size);
    
    for(int i=0; i<pool_size; ++i) {
        struct whisper_state* state = whisper_init_state(ctx_);
        if(!state) throw std::runtime_error("Failed to allocate whisper state");
        state_pool_.push(state);
        all_states_.push_back(state);
    }

    if (settings_.enable_vad) {
        std::string vad_path = settings_.model_dir + "/" + settings_.vad_model_filename;
        struct whisper_vad_context_params vparams = whisper_vad_default_context_params();
        vparams.use_gpu = false; 
        vad_ctx_ = whisper_vad_init_from_file_with_params(vad_path.c_str(), vparams);
        if (vad_ctx_) spdlog::info("✅ Native Silero VAD loaded successfully (CPU Mode).");
    }
}

SttEngine::~SttEngine() {
    for(auto* state : all_states_) whisper_free_state(state);
    if (ctx_) whisper_free(ctx_);
    if (vad_ctx_) whisper_vad_free(vad_ctx_);
}

bool SttEngine::is_ready() const { return ctx_ != nullptr; }

struct whisper_state* SttEngine::acquire_state() {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    pool_cv_.wait(lock, [this]{ return !state_pool_.empty(); });
    struct whisper_state* state = state_pool_.front();
    state_pool_.pop();
    return state;
}

void SttEngine::release_state(struct whisper_state* state) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    state_pool_.push(state);
    pool_cv_.notify_one();
}

std::vector<float> SttEngine::resample_audio(const std::vector<float>& input, int src_rate, int target_rate) {
    if (src_rate == target_rate || input.empty()) return input;
    double ratio = (double)target_rate / (double)src_rate;
    long output_frames = (long)(input.size() * ratio) + 100; 
    std::vector<float> output(output_frames);
    SRC_DATA src_data;
    src_data.data_in = input.data();
    src_data.input_frames = (long)input.size();
    src_data.data_out = output.data();
    src_data.output_frames = output_frames;
    src_data.src_ratio = ratio;
    src_data.end_of_input = 0; 
    int error = src_simple(&src_data, SRC_SINC_FASTEST, 1); 
    if (error) return input; 
    output.resize(src_data.output_frames_gen);
    return output;
}

bool SttEngine::is_speech_detected(const std::vector<float>& pcmf32) {
    if (!vad_ctx_) return true; 
    std::lock_guard<std::mutex> lock(vad_mutex_);
    return whisper_vad_detect_speech(vad_ctx_, pcmf32.data(), (int)pcmf32.size());
}

std::vector<TranscriptionResult> SttEngine::transcribe_pcm16(
    const std::vector<int16_t>& pcm16, 
    int input_sample_rate,
    const RequestOptions& options
) {
    std::vector<float> pcmf32(pcm16.size());
    for (size_t i = 0; i < pcm16.size(); ++i) {
        pcmf32[i] = static_cast<float>(pcm16[i]) / 32768.0f;
    }
    return transcribe(pcmf32, input_sample_rate, options);
}

std::vector<TranscriptionResult> SttEngine::transcribe(
    const std::vector<float>& pcmf32, 
    int input_sample_rate,
    const RequestOptions& options
) {
    if (!ctx_) return {};

    std::vector<float> processed_audio;
    if (input_sample_rate != 16000) {
        processed_audio = resample_audio(pcmf32, input_sample_rate, 16000);
    } else {
        processed_audio = pcmf32;
    }

    if (settings_.enable_vad && processed_audio.size() > (16000 * 0.2)) { 
        if (!is_speech_detected(processed_audio)) {
            TranscriptionResult empty_res;
            empty_res.text = "";
            empty_res.language = "unknown";
            empty_res.prob = 0.0f;
            empty_res.t0 = 0;
            empty_res.t1 = (int64_t)((double)processed_audio.size() / 16.0); 
            empty_res.speaker_turn_next = false;
            return {empty_res}; 
        }
    }

    struct whisper_state* state = acquire_state();

    // Strateji Belirleme (Config vs Request)
    int active_beam_size = (options.beam_size >= 0) ? options.beam_size : settings_.beam_size;
    float active_temp = (options.temperature >= 0.0f) ? options.temperature : settings_.temperature;
    int active_best_of = (options.best_of >= 0) ? options.best_of : settings_.best_of;

    whisper_sampling_strategy strategy = (active_beam_size > 1) ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY;
    whisper_full_params wparams = whisper_full_default_params(strategy);
    
    wparams.print_realtime = false;
    wparams.print_progress = false;
    wparams.print_timestamps = !settings_.no_timestamps;
    wparams.print_special = false;
    wparams.token_timestamps = true; 
    wparams.suppress_nst = settings_.suppress_nst; 
    wparams.no_speech_thold = settings_.no_speech_threshold; 
    
    // --- REQUEST OVERRIDES ---
    wparams.translate = options.translate; // Request öncelikli (default false)
    wparams.tdrz_enable = options.enable_diarization; // Request öncelikli

    std::string target_lang = options.language;
    if (target_lang.empty()) target_lang = settings_.language;
    wparams.language = target_lang.c_str();    
    
    if (!options.prompt.empty()) {
        wparams.initial_prompt = options.prompt.c_str();
    }
    
    wparams.temperature = active_temp;
    
    if (strategy == WHISPER_SAMPLING_BEAM_SEARCH) {
        wparams.beam_search.beam_size = active_beam_size;
    } else {
        wparams.greedy.best_of = active_best_of;
    }
    wparams.entropy_thold = 2.40f;
    wparams.logprob_thold = settings_.logprob_threshold;
    wparams.n_threads = settings_.n_threads;

    int ret = whisper_full_with_state(ctx_, state, wparams, processed_audio.data(), processed_audio.size());
    
    std::vector<TranscriptionResult> results;
    
    if (ret == 0) {
        const int n_segments = whisper_full_n_segments_from_state(state);
        const float MIN_AVG_TOKEN_PROB = 0.40f; 
        
        for (int i = 0; i < n_segments; ++i) {
            const char* text = whisper_full_get_segment_text_from_state(state, i);
            const int64_t t0 = whisper_full_get_segment_t0_from_state(state, i);
            const int64_t t1 = whisper_full_get_segment_t1_from_state(state, i);
            bool speaker_turn = whisper_full_get_segment_speaker_turn_next_from_state(state, i);

            std::vector<TokenData> tokens;
            int n_tokens = whisper_full_n_tokens_from_state(state, i);
            double total_prob = 0.0;
            int valid_token_count = 0;
            
            for (int j = 0; j < n_tokens; ++j) {
                auto data = whisper_full_get_token_data_from_state(state, i, j);
                const char* token_text = whisper_token_to_str(ctx_, data.id);
                if (data.id >= whisper_token_eot(ctx_)) continue; 

                tokens.push_back({std::string(token_text), data.p, data.t0, data.t1});
                total_prob += data.p;
                valid_token_count++;
            }

            float avg_prob = (valid_token_count > 0) ? (float)(total_prob / valid_token_count) : 0.0f;
            if (avg_prob < MIN_AVG_TOKEN_PROB && valid_token_count > 0) continue;

            results.push_back({std::string(text), target_lang, avg_prob, t0, t1, speaker_turn, tokens});
        }
    } else {
        spdlog::error("Whisper processing failed.");
    }

    release_state(state);
    return results;
}
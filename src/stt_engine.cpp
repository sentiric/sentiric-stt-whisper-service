#include "stt_engine.h"
#include "spdlog/spdlog.h"
#include <samplerate.h>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <numeric>

SttEngine::SttEngine(const Settings& settings) : settings_(settings) {
    // 1. Ana Model Yükleme (Shared Context)
    std::string model_path = settings_.model_dir + "/" + settings_.model_filename;
    spdlog::info("Loading Whisper model from: {}", model_path);

    struct whisper_context_params cparams = whisper_context_default_params();
    
    #ifdef GGML_USE_CUDA
    spdlog::info("CUDA detected. Enabling GPU offloading for Main Engine.");
    cparams.use_gpu = true;
    if (settings_.flash_attn) {
        spdlog::info("⚡ Flash Attention enabled.");
        cparams.flash_attn = true; 
    }
    #endif

    ctx_ = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    if (!ctx_) {
        spdlog::error("Failed to initialize whisper context from {}", model_path);
        throw std::runtime_error("Whisper model initialization failed");
    }

    // 2. Dynamic Batching: State Havuzu Oluşturma
    // Her state, ayrı bir GPU stream'i üzerinde çalışabilir.
    int pool_size = settings_.parallel_requests;
    if (pool_size < 1) pool_size = 1;
    
    spdlog::info("⚡ Initializing Dynamic Batching Pool with {} states...", pool_size);
    
    for(int i=0; i<pool_size; ++i) {
        struct whisper_state* state = whisper_init_state(ctx_);
        if(!state) {
            throw std::runtime_error("Failed to allocate whisper state (VRAM might be full)");
        }
        state_pool_.push(state);
        all_states_.push_back(state);
    }
    spdlog::info("✅ State pool initialized. Max concurrency: {}", pool_size);

    // 3. VAD Modeli (CPU)
    if (settings_.enable_vad) {
        std::string vad_path = settings_.model_dir + "/" + settings_.vad_model_filename;
        spdlog::info("Loading VAD model from: {}", vad_path);
        
        struct whisper_vad_context_params vparams = whisper_vad_default_context_params();
        vparams.use_gpu = false; // CPU Zorunlu
        
        vad_ctx_ = whisper_vad_init_from_file_with_params(vad_path.c_str(), vparams);
        
        if (!vad_ctx_) {
            spdlog::warn("⚠️ VAD model could not be loaded. Continuing without VAD.");
        } else {
            spdlog::info("✅ Native Silero VAD loaded successfully (CPU Mode).");
        }
    }
}

SttEngine::~SttEngine() {
    // Havuzu temizle
    for(auto* state : all_states_) {
        whisper_free_state(state);
    }
    
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
    if (vad_ctx_) {
        whisper_vad_free(vad_ctx_);
        vad_ctx_ = nullptr;
    }
}

bool SttEngine::is_ready() const {
    return ctx_ != nullptr;
}

// --- Pool Management ---
struct whisper_state* SttEngine::acquire_state() {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    // Havuzda boş yer olana kadar bekle
    pool_cv_.wait(lock, [this]{ return !state_pool_.empty(); });
    
    struct whisper_state* state = state_pool_.front();
    state_pool_.pop();
    return state;
}

void SttEngine::release_state(struct whisper_state* state) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    state_pool_.push(state);
    pool_cv_.notify_one(); // Bekleyen varsa uyandır
}
// -----------------------

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
    std::lock_guard<std::mutex> lock(vad_mutex_); // VAD için hafif kilit
    return whisper_vad_detect_speech(vad_ctx_, pcmf32.data(), (int)pcmf32.size());
}

std::vector<TranscriptionResult> SttEngine::transcribe_pcm16(
    const std::vector<int16_t>& pcm16, 
    int input_sample_rate,
    const std::string& language
) {
    std::vector<float> pcmf32(pcm16.size());
    for (size_t i = 0; i < pcm16.size(); ++i) {
        pcmf32[i] = static_cast<float>(pcm16[i]) / 32768.0f;
    }
    return transcribe(pcmf32, input_sample_rate, language);
}

std::vector<TranscriptionResult> SttEngine::transcribe(
    const std::vector<float>& pcmf32, 
    int input_sample_rate,
    const std::string& language
) {
    // Bu fonksiyon artık "Global Mutex" kullanmıyor!
    // Böylece aynı anda birden fazla thread buraya girebilir.

    if (!ctx_) return {};

    // 1. Pre-processing (Thread-Local)
    std::vector<float> processed_audio;
    if (input_sample_rate != 16000) {
        processed_audio = resample_audio(pcmf32, input_sample_rate, 16000);
    } else {
        processed_audio = pcmf32;
    }

    // 2. VAD Check (CPU - Fast - protected by vad_mutex)
    if (settings_.enable_vad && processed_audio.size() > (16000 * 0.2)) { 
        if (!is_speech_detected(processed_audio)) {
            // Sessizlik için erken dönüş
            TranscriptionResult empty_res;
            empty_res.text = "";
            empty_res.language = "unknown";
            empty_res.prob = 0.0f;
            empty_res.t0 = 0;
            empty_res.t1 = (int64_t)((double)processed_audio.size() / 16.0); 
            return {empty_res}; 
        }
    }

    // 3. Acquire State (Concurrency Bottleneck - only if pool is empty)
    struct whisper_state* state = acquire_state();

    // 4. Inference (GPU - Parallel Exec)
    whisper_sampling_strategy strategy = (settings_.beam_size > 1) ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY;
    whisper_full_params wparams = whisper_full_default_params(strategy);
    
    // ... (Params aynı) ...
    wparams.print_realtime = false;
    wparams.print_progress = false;
    wparams.print_timestamps = !settings_.no_timestamps;
    wparams.print_special = false;
    wparams.token_timestamps = true; 
    wparams.suppress_nst = settings_.suppress_nst; 
    wparams.no_speech_thold = settings_.no_speech_threshold; 
    wparams.translate = settings_.translate;
    wparams.n_threads = settings_.n_threads;

    std::string target_lang = language;
    if (target_lang.empty()) target_lang = settings_.language;
    wparams.language = target_lang.c_str();    
    wparams.temperature = settings_.temperature;
    
    if (strategy == WHISPER_SAMPLING_BEAM_SEARCH) {
        wparams.beam_search.beam_size = settings_.beam_size;
    } else {
        wparams.greedy.best_of = settings_.best_of;
    }
    wparams.entropy_thold = 2.40f;
    wparams.logprob_thold = settings_.logprob_threshold;

    // KRİTİK DEĞİŞİKLİK: whisper_full yerine whisper_full_with_state
    int ret = whisper_full_with_state(ctx_, state, wparams, processed_audio.data(), processed_audio.size());
    
    std::vector<TranscriptionResult> results;
    
    if (ret == 0) {
        const int n_segments = whisper_full_n_segments_from_state(state);
        const float MIN_AVG_TOKEN_PROB = 0.40f; 
        
        for (int i = 0; i < n_segments; ++i) {
            const char* text = whisper_full_get_segment_text_from_state(state, i);
            const int64_t t0 = whisper_full_get_segment_t0_from_state(state, i);
            const int64_t t1 = whisper_full_get_segment_t1_from_state(state, i);
            
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

            results.push_back({std::string(text), target_lang, avg_prob, t0, t1, tokens});
        }
    } else {
        spdlog::error("Whisper processing failed.");
    }

    // 5. Release State
    release_state(state);
    
    return results;
}
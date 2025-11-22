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

    // --- YENİ: Context Params (v1.8.2) ---
    struct whisper_context_params cparams = whisper_context_default_params();
    
    #ifdef GGML_USE_CUDA
    spdlog::info("CUDA detected. Enabling GPU offloading.");
    cparams.use_gpu = true;
    // YENİ: Flash Attention
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
    
    spdlog::info("Whisper model loaded successfully.");
}

// ... (Destructor ve Resample fonksiyonları aynı) ...
SttEngine::~SttEngine() {
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

bool SttEngine::is_ready() const {
    return ctx_ != nullptr;
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
    if (error) {
        spdlog::error("Libsamplerate error: {}", src_strerror(error));
        return input; 
    }
    output.resize(src_data.output_frames_gen);
    return output;
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
    if (input_sample_rate != 16000) {
        pcmf32 = resample_audio(pcmf32, input_sample_rate, 16000);
    }
    return transcribe(pcmf32, 16000, language);
}

std::vector<TranscriptionResult> SttEngine::transcribe(
    const std::vector<float>& pcmf32, 
    int input_sample_rate,
    const std::string& language
) {
    std::lock_guard<std::mutex> lock(mutex_); 

    if (!ctx_) return {};

    std::vector<float> processed_audio;
    if (input_sample_rate != 16000) {
        processed_audio = resample_audio(pcmf32, input_sample_rate, 16000);
    } else {
        processed_audio = pcmf32;
    }

    // --- YENİ: VAD Kontrolü (Opsiyonel ama önerilir) ---
    // VAD modelini henüz yüklemediğimiz için (ModelManager güncellemesi gerekir),
    // şimdilik Whisper'ın kendi "no_speech_thold" özelliğine güveniyoruz.
    // İleride buraya `whisper_vad_detect_speech` eklenecek.

    // Whisper Params
    whisper_sampling_strategy strategy = (settings_.beam_size > 1) ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY;
    whisper_full_params wparams = whisper_full_default_params(strategy);
    
    wparams.print_realtime = false;
    wparams.print_progress = false;
    wparams.print_timestamps = !settings_.no_timestamps;
    wparams.print_special = false;
    wparams.token_timestamps = true; 
    
    // --- v1.8.2 İYİLEŞTİRMELERİ ---
    wparams.suppress_nst = settings_.suppress_nst; // [Music] vb. engelle
    wparams.no_speech_thold = settings_.no_speech_threshold; // Sessizlik eşiği (0.6)
    // ------------------------------

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

    if (whisper_full(ctx_, wparams, processed_audio.data(), processed_audio.size()) != 0) {
        spdlog::error("Whisper failed to process audio");
        return {};
    }
    
    std::vector<TranscriptionResult> results;
    const int n_segments = whisper_full_n_segments(ctx_);
    
    // --- GÜVEN FİLTRESİ (Confidence Filter) ---
    const float MIN_AVG_TOKEN_PROB = 0.40f; 
    
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx_, i);
        const int64_t t0 = whisper_full_get_segment_t0(ctx_, i);
        const int64_t t1 = whisper_full_get_segment_t1(ctx_, i);
        
        // YENİ: Segment bazlı sessizlik kontrolü (v1.8.2 API)
        // float no_speech = whisper_full_get_segment_no_speech_prob(ctx_, i);
        // if (no_speech > settings_.no_speech_threshold) continue;

        std::vector<TokenData> tokens;
        int n_tokens = whisper_full_n_tokens(ctx_, i);
        double total_prob = 0.0;
        int valid_token_count = 0;
        
        for (int j = 0; j < n_tokens; ++j) {
            auto data = whisper_full_get_token_data(ctx_, i, j);
            const char* token_text = whisper_token_to_str(ctx_, data.id);
            if (data.id >= whisper_token_eot(ctx_)) continue; 

            tokens.push_back({std::string(token_text), data.p, data.t0, data.t1});
            total_prob += data.p;
            valid_token_count++;
        }

        float avg_prob = (valid_token_count > 0) ? (float)(total_prob / valid_token_count) : 0.0f;
        
        if (avg_prob < MIN_AVG_TOKEN_PROB && valid_token_count > 0) {
            spdlog::warn("🗑️ Discarded hallucination (Avg Prob: {:.2f}): {}", avg_prob, text);
            continue;
        }

        results.push_back({std::string(text), target_lang, avg_prob, t0, t1, tokens});
    }
    
    return results;
}
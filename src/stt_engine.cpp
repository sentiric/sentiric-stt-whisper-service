#include "stt_engine.h"
#include "spdlog/spdlog.h"
#include <samplerate.h> // Libsamplerate
#include <stdexcept>
#include <cmath>
#include <algorithm>

SttEngine::SttEngine(const Settings& settings) : settings_(settings) {
    std::string model_path = settings_.model_dir + "/" + settings_.model_filename;
    spdlog::info("Loading Whisper model from: {}", model_path);

    // Whisper context'i başlat
    struct whisper_context_params cparams = whisper_context_default_params();
    
    // GPU desteği varsa kullan
    #ifdef GGML_USE_CUDA
    spdlog::info("CUDA detected. Enabling GPU offloading for Whisper.");
    cparams.use_gpu = true;
    #endif

    ctx_ = whisper_init_from_file_with_params(model_path.c_str(), cparams);

    if (!ctx_) {
        spdlog::error("Failed to initialize whisper context from {}", model_path);
        throw std::runtime_error("Whisper model initialization failed");
    }
    
    spdlog::info("Whisper model loaded successfully.");
}

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
    if (src_rate == target_rate || input.empty()) {
        return input;
    }

    double ratio = (double)target_rate / (double)src_rate;
    long output_frames = (long)(input.size() * ratio) + 100; // Biraz buffer ekle
    std::vector<float> output(output_frames);

    SRC_DATA src_data;
    src_data.data_in = input.data();
    src_data.input_frames = (long)input.size();
    src_data.data_out = output.data();
    src_data.output_frames = output_frames;
    src_data.src_ratio = ratio;
    src_data.end_of_input = 0; // Batch mode için basit ayar

    // SRC_SINC_FASTEST: Hızlı ve yeterince kaliteli
    int error = src_simple(&src_data, SRC_SINC_FASTEST, 1); // 1 channel (mono)

    if (error) {
        spdlog::error("Libsamplerate error: {}", src_strerror(error));
        return input; // Hata durumunda orijinali döndür
    }

    output.resize(src_data.output_frames_gen);
    spdlog::debug("Resampled audio from {}Hz to {}Hz ({} -> {} frames)", src_rate, target_rate, input.size(), output.size());
    return output;
}

std::vector<TranscriptionResult> SttEngine::transcribe_pcm16(
    const std::vector<int16_t>& pcm16, 
    int input_sample_rate,
    const std::string& language
) {
    // 1. Normalization
    std::vector<float> pcmf32(pcm16.size());
    for (size_t i = 0; i < pcm16.size(); ++i) {
        pcmf32[i] = static_cast<float>(pcm16[i]) / 32768.0f;
    }

    // 2. Resampling
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

    // Resampling Logic
    std::vector<float> processed_audio;
    if (input_sample_rate != 16000) {
        processed_audio = resample_audio(pcmf32, input_sample_rate, 16000);
    } else {
        processed_audio = pcmf32;
    }

    // Whisper Params
    whisper_sampling_strategy strategy = (settings_.beam_size > 1) ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY;
    whisper_full_params wparams = whisper_full_default_params(strategy);
    
    wparams.print_realtime = false;
    wparams.print_progress = false;
    wparams.print_timestamps = !settings_.no_timestamps;
    wparams.print_special = false;
    wparams.token_timestamps = true; // KRİTİK: Token (kelime) bazlı timestamp'i aç
    
    wparams.translate = settings_.translate;
    wparams.n_threads = settings_.n_threads;

    // Dil Seçimi
    std::string target_lang = language;
    if (target_lang.empty()) {
        target_lang = settings_.language;
    }    
    wparams.language = settings_.language.c_str();    
    
    // Advanced Settings
    wparams.temperature = settings_.temperature;
    
    if (strategy == WHISPER_SAMPLING_BEAM_SEARCH) {
        wparams.beam_search.beam_size = settings_.beam_size;
    } else {
        wparams.greedy.best_of = settings_.best_of;
    }

    wparams.entropy_thold = 2.40f;
    wparams.logprob_thold = settings_.logprob_threshold;
    wparams.no_speech_thold = settings_.no_speech_threshold;

    // Inference
    if (whisper_full(ctx_, wparams, processed_audio.data(), processed_audio.size()) != 0) {
        spdlog::error("Whisper failed to process audio");
        return {};
    }
    
    // Sonuç Toplama
    std::vector<TranscriptionResult> results;
    const int n_segments = whisper_full_n_segments(ctx_);
    
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx_, i);
        const int64_t t0 = whisper_full_get_segment_t0(ctx_, i);
        const int64_t t1 = whisper_full_get_segment_t1(ctx_, i);
        
        // --- YENİ: Token Extraction ---
        std::vector<TokenData> tokens;
        int n_tokens = whisper_full_n_tokens(ctx_, i);
        
        for (int j = 0; j < n_tokens; ++j) {
            auto data = whisper_full_get_token_data(ctx_, i, j);
            const char* token_text = whisper_token_to_str(ctx_, data.id);
            
            // Özel tokenları filtrele (örn: [BEG], [NOTIMESTAMP] vb.)
            if (data.id >= whisper_token_eot(ctx_)) continue; 

            tokens.push_back({
                std::string(token_text),
                data.p,
                data.t0,
                data.t1
            });
        }
        // ------------------------------

        results.push_back({std::string(text), target_lang, 1.0f, t0, t1, tokens});
    }
    
    return results;
}
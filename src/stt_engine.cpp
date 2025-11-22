#include "stt_engine.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <cmath>

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

std::vector<TranscriptionResult> SttEngine::transcribe_pcm16(const std::vector<int16_t>& pcm16) {
    // 16-bit INT -> 32-bit FLOAT dönüşümü (normalize)
    // Whisper 32-bit float ve -1.0 ile 1.0 arasında değer bekler.
    std::vector<float> pcmf32(pcm16.size());
    for (size_t i = 0; i < pcm16.size(); ++i) {
        pcmf32[i] = static_cast<float>(pcm16[i]) / 32768.0f;
    }
    return transcribe(pcmf32);
}

std::vector<TranscriptionResult> SttEngine::transcribe(const std::vector<float>& pcmf32) {
    std::lock_guard<std::mutex> lock(mutex_); // Context'i kilitle

    if (!ctx_) return {};

    // Whisper parametrelerini ayarla
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    
    wparams.strategy = WHISPER_SAMPLING_GREEDY;
    wparams.print_realtime = false;
    wparams.print_progress = false;
    wparams.print_timestamps = !settings_.no_timestamps;
    wparams.print_special = false;
    wparams.translate = settings_.translate;
    wparams.language = settings_.language.c_str();
    wparams.n_threads = settings_.n_threads;
    
    // VAD (Voice Activity Detection) - Boş sesleri atla
    wparams.no_speech_thold = 0.6f; 

    // Transkripsiyonu çalıştır
    if (whisper_full(ctx_, wparams, pcmf32.data(), pcmf32.size()) != 0) {
        spdlog::error("Whisper failed to process audio");
        return {};
    }

    std::vector<TranscriptionResult> results;
    const int n_segments = whisper_full_n_segments(ctx_);
    
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx_, i);
        const int64_t t0 = whisper_full_get_segment_t0(ctx_, i);
        const int64_t t1 = whisper_full_get_segment_t1(ctx_, i);
        
        // Olasılık (confidence) hesabı
        // Şimdilik dummy 1.0 veriyoruz, ileride token olasılıkları hesaplanabilir.
        float prob = 1.0f; 

        results.push_back({std::string(text), settings_.language, prob, t0, t1});
    }

    return results;
}
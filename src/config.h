#pragma once

#include <string>
#include <cstdlib>
#include <thread>
#include <algorithm>
#include "spdlog/spdlog.h"

struct Settings {
    std::string host = "0.0.0.0";
    int http_port = 15030;
    int grpc_port = 15031;
    int metrics_port = 15032;

    // --- Main Model ---
    std::string model_dir = "/models";
    std::string model_filename = "ggml-medium.bin";
    std::string model_url_template = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-{model_name}.bin";
    int model_load_timeout = 600;

    // --- VAD Settings (NİHAİ DÜZELTME) ---
    // Yerelde kullanılacak dosya adı (Kod bu ismi arar)
    std::string vad_model_filename = "ggml-silero-vad.bin"; 
    
    // İndirilecek Kaynak URL (ggml-org/whisper-vad reposu - v6.2.0)
    std::string vad_model_url = "https://huggingface.co/ggml-org/whisper-vad/resolve/main/ggml-silero-v6.2.0.bin";
    
    bool enable_vad = true;
    float vad_threshold = 0.5f;        
    int vad_ms_min_duration = 500;     
    
    int n_threads = std::min(4, (int)std::thread::hardware_concurrency());
    std::string device = "auto"; 
    std::string compute_type = "int8";

    std::string language = "auto"; 
    bool translate = false;
    bool no_timestamps = false;
    
    int beam_size = 5;
    float temperature = 0.0f;
    int best_of = 5;
    float logprob_threshold = -1.0f;
    float no_speech_threshold = 0.6f;
    
    bool flash_attn = true;
    bool suppress_nst = true; 

    int sample_rate = 16000; 
    std::string log_level = "info";
    std::string grpc_ca_path = "";
    std::string grpc_cert_path = "";
    std::string grpc_key_path = "";
};

inline Settings load_settings() {
    Settings s;
    auto get_env = [](const char* name, const std::string& def) -> std::string {
        const char* val = std::getenv(name);
        return val ? std::string(val) : def;
    };
    auto get_int = [&](const char* name, int def) -> int {
        const char* val = std::getenv(name);
        return val ? std::stoi(val) : def;
    };
    auto get_float = [&](const char* name, float def) -> float {
        const char* val = std::getenv(name);
        return val ? std::stof(val) : def;
    };
    auto get_bool = [&](const char* name, bool def) -> bool {
        const char* val = std::getenv(name);
        if (!val) return def;
        std::string v = val;
        std::transform(v.begin(), v.end(), v.begin(), ::tolower);
        return v == "true" || v == "1";
    };

    s.host = get_env("STT_WHISPER_SERVICE_IPV4_ADDRESS", "0.0.0.0");
    s.http_port = get_int("STT_WHISPER_SERVICE_HTTP_PORT", s.http_port);
    s.grpc_port = get_int("STT_WHISPER_SERVICE_GRPC_PORT", s.grpc_port);
    s.metrics_port = get_int("STT_WHISPER_SERVICE_METRICS_PORT", s.metrics_port);
    
    s.model_dir = get_env("STT_WHISPER_SERVICE_MODEL_DIR", s.model_dir);
    std::string size = get_env("STT_WHISPER_SERVICE_MODEL_SIZE", "medium");
    s.model_filename = get_env("STT_WHISPER_SERVICE_MODEL_FILENAME", "ggml-" + size + ".bin");
    
    // --- VAD Config ---
    s.vad_model_filename = get_env("STT_WHISPER_SERVICE_VAD_MODEL", "ggml-silero-vad.bin");
    s.vad_model_url = get_env("STT_WHISPER_SERVICE_VAD_URL", s.vad_model_url);
    s.enable_vad = get_bool("STT_WHISPER_SERVICE_ENABLE_VAD", s.enable_vad);
    s.vad_threshold = get_float("STT_WHISPER_SERVICE_VAD_THRESHOLD", s.vad_threshold);
    
    s.flash_attn = get_bool("STT_WHISPER_SERVICE_FLASH_ATTN", s.flash_attn);
    s.suppress_nst = get_bool("STT_WHISPER_SERVICE_SUPPRESS_NST", s.suppress_nst);

    s.n_threads = get_int("STT_WHISPER_SERVICE_THREADS", s.n_threads);
    s.language = get_env("STT_WHISPER_SERVICE_LANGUAGE", s.language);
    s.translate = get_bool("STT_WHISPER_SERVICE_TRANSLATE", s.translate);
    
    s.beam_size = get_int("STT_WHISPER_SERVICE_BEAM_SIZE", s.beam_size);
    s.temperature = get_float("STT_WHISPER_SERVICE_TEMPERATURE", s.temperature);
    s.best_of = get_int("STT_WHISPER_SERVICE_BEST_OF", s.best_of);
    s.logprob_threshold = get_float("STT_WHISPER_SERVICE_LOGPROB_THRESHOLD", s.logprob_threshold);
    s.no_speech_threshold = get_float("STT_WHISPER_SERVICE_NO_SPEECH_THRESHOLD", s.no_speech_threshold);

    s.log_level = get_env("STT_WHISPER_SERVICE_LOG_LEVEL", s.log_level);
    s.grpc_ca_path = get_env("STT_WHISPER_SERVICE_CA_PATH", s.grpc_ca_path);
    s.grpc_cert_path = get_env("STT_WHISPER_SERVICE_CERT_PATH", s.grpc_cert_path);
    s.grpc_key_path = get_env("STT_WHISPER_SERVICE_KEY_PATH", s.grpc_key_path);

    return s;
}
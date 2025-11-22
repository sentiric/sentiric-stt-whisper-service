#pragma once

#include <string>
#include <cstdlib>
#include <thread>
#include <algorithm>
#include "spdlog/spdlog.h"

struct Settings {
    // Network
    std::string host = "0.0.0.0";
    int http_port = 15030;
    int grpc_port = 15031;

    // Model
    std::string model_dir = "/models";
    std::string model_filename = "ggml-base.bin"; // Varsayılan
    
    // STT Engine Settings
    int n_threads = std::min(4, (int)std::thread::hardware_concurrency());
    std::string language = "auto"; // "tr", "en" vs.
    bool translate = false;
    bool no_timestamps = false;
    
    // Audio Processing
    int sample_rate = 16000; // Whisper standardı
    
    // Logging
    std::string log_level = "info";

    // Security (mTLS)
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
    auto get_bool = [&](const char* name, bool def) -> bool {
        const char* val = std::getenv(name);
        if (!val) return def;
        std::string v = val;
        std::transform(v.begin(), v.end(), v.begin(), ::tolower);
        return v == "true" || v == "1";
    };

    // Network
    s.host = get_env("STT_WHISPER_SERVICE_LISTEN_ADDRESS", s.host);
    s.http_port = get_int("STT_WHISPER_SERVICE_HTTP_PORT", s.http_port);
    s.grpc_port = get_int("STT_WHISPER_SERVICE_GRPC_PORT", s.grpc_port);

    // Model
    s.model_dir = get_env("STT_WHISPER_SERVICE_MODEL_DIR", s.model_dir);
    s.model_filename = get_env("STT_WHISPER_SERVICE_MODEL_FILENAME", s.model_filename);

    // Engine
    s.n_threads = get_int("STT_WHISPER_SERVICE_THREADS", s.n_threads);
    s.language = get_env("STT_WHISPER_SERVICE_LANGUAGE", s.language);
    s.translate = get_bool("STT_WHISPER_SERVICE_TRANSLATE", s.translate);

    // Logging
    s.log_level = get_env("STT_WHISPER_SERVICE_LOG_LEVEL", s.log_level);

    // Security
    s.grpc_ca_path = get_env("GRPC_TLS_CA_PATH", s.grpc_ca_path);
    s.grpc_cert_path = get_env("STT_WHISPER_SERVICE_CERT_PATH", s.grpc_cert_path);
    s.grpc_key_path = get_env("STT_WHISPER_SERVICE_KEY_PATH", s.grpc_key_path);

    return s;
}
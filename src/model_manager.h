#pragma once

#include "config.h"
#include <string>

class ModelManager {
public:
    // Ana modeli (Whisper) kontrol eder/indirir
    static std::string ensure_model(const Settings& settings);

    // VAD modelini (Silero) kontrol eder/indirir
    static std::string ensure_vad_model(const Settings& settings);

private:
    // Genel amaçlı dosya doğrulama ve indirme
    static std::string ensure_file(const std::string& dir, const std::string& filename, const std::string& url, size_t min_size_bytes = 1024 * 1024);
    
    static bool download_file(const std::string& url, const std::string& filepath);
};
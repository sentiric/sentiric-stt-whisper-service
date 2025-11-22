#pragma once

#include "config.h"
#include "whisper.h"
#include <vector>
#include <string>
#include <mutex>
#include <memory>

struct TranscriptionResult {
    std::string text;
    std::string language;
    float prob;
    int64_t t0; // Başlangıç zamanı (ms)
    int64_t t1; // Bitiş zamanı (ms)
};

class SttEngine {
public:
    explicit SttEngine(const Settings& settings);
    ~SttEngine();

    // Modelin yüklü olup olmadığını kontrol eder
    bool is_ready() const;

    // PCM verisini (32-bit float) işler ve metin döndürür
    std::vector<TranscriptionResult> transcribe(const std::vector<float>& pcmf32);

    // Raw 16-bit PCM verisini işler (dönüştürme yapar)
    std::vector<TranscriptionResult> transcribe_pcm16(const std::vector<int16_t>& pcm16);

private:
    Settings settings_;
    struct whisper_context* ctx_ = nullptr;
    std::mutex mutex_; // Whisper context thread-safe değildir, korumamız lazım
};
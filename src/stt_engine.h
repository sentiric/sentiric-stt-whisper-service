#pragma once

#include "config.h"
#include "whisper.h"
#include <vector>
#include <string>
#include <mutex>
#include <memory>

// Kelime/Token seviyesinde detay
struct TokenData {
    std::string text;
    float p;       // Olasılık (Probability)
    int64_t t0;    // Başlangıç (ms)
    int64_t t1;    // Bitiş (ms)
};

struct TranscriptionResult {
    std::string text;
    std::string language;
    float prob;
    int64_t t0; // Segment Başlangıç (ms)
    int64_t t1; // Segment Bitiş (ms)
    std::vector<TokenData> tokens; // YENİ: Kelime bazlı veriler
};

class SttEngine {
public:
    explicit SttEngine(const Settings& settings);
    ~SttEngine();

    // Modelin yüklü olup olmadığını kontrol eder
    bool is_ready() const;

    // PCM verisini (32-bit float) işler ve metin döndürür
    // input_sample_rate: Verinin orijinal örnekleme hızı (varsayılan 16000)
    std::vector<TranscriptionResult> transcribe(
        const std::vector<float>& pcmf32, 
        int input_sample_rate = 16000,
        const std::string& language = "" 
    );

    // Raw 16-bit PCM verisini işler (dönüştürme ve resampling yapar)
    std::vector<TranscriptionResult> transcribe_pcm16(
        const std::vector<int16_t>& pcm16, 
        int input_sample_rate = 16000,
        const std::string& language = ""
    );

private:
    // Ses verisini yeniden örnekler (Resampling)
    std::vector<float> resample_audio(const std::vector<float>& input, int src_rate, int target_rate);

    Settings settings_;
    struct whisper_context* ctx_ = nullptr;
    std::mutex mutex_; // Whisper context thread-safe değildir, korumamız lazım
};
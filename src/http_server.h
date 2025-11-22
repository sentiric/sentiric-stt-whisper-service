#pragma once

#include "stt_engine.h"
#include <memory>
#include <string>
#include <vector>
#include "httplib.h"
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/histogram.h>
#include <prometheus/gauge.h>

// Uygulama genelinde kullanılacak metrikler
struct AppMetrics {
    prometheus::Counter& requests_total;
    prometheus::Histogram& request_latency;
    prometheus::Counter& audio_seconds_processed_total;
};

// İşlenmiş ses verisi yapısı
struct DecodedAudio {
    std::vector<int16_t> pcm_data;
    int sample_rate = 16000;
    int channels = 1;
};

class MetricsServer {
public:
    MetricsServer(const std::string& host, int port, prometheus::Registry& registry);
    void run();
    void stop();

private:
    httplib::Server svr_;
    std::string host_;
    int port_;
    prometheus::Registry& registry_;
};

class HttpServer {
public:
    // Metrics referansını da alıyoruz ki REST isteklerini de sayabilelim
    HttpServer(std::shared_ptr<SttEngine> engine, AppMetrics& metrics, const std::string& host, int port);
    void run();
    void stop();

private:
    void setup_routes();
    
    // Güvenli WAV parser ve Mono Converter
    DecodedAudio parse_wav_robust(const std::string& bytes);

    httplib::Server svr_;
    std::shared_ptr<SttEngine> engine_;
    AppMetrics& metrics_;
    std::string host_;
    int port_;
};
#pragma once

#include "stt_engine.h"
#include <memory>
#include <string>
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
    HttpServer(std::shared_ptr<SttEngine> engine, const std::string& host, int port);
    void run();
    void stop();

private:
    httplib::Server svr_;
    std::shared_ptr<SttEngine> engine_;
    std::string host_;
    int port_;
};
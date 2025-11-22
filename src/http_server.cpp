#include "http_server.h"
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"
#include <prometheus/text_serializer.h>
#include <sstream>
#include <cstring>

using json = nlohmann::json;

// --- Metrics Server ---
MetricsServer::MetricsServer(const std::string& host, int port, prometheus::Registry& registry)
    : host_(host), port_(port), registry_(registry) {
    svr_.Get("/metrics", [this](const httplib::Request &, httplib::Response &res) {
        prometheus::TextSerializer serializer;
        auto collected_metrics = this->registry_.Collect();
        std::stringstream ss;
        serializer.Serialize(ss, collected_metrics);
        res.set_content(ss.str(), "text/plain; version=0.0.4");
    });
}

void MetricsServer::run() {
    spdlog::info("📊 Metrics server listening on {}:{}", host_, port_);
    svr_.listen(host_.c_str(), port_);
}

void MetricsServer::stop() {
    if (svr_.is_running()) svr_.stop();
}

// --- Main HTTP Server ---
HttpServer::HttpServer(std::shared_ptr<SttEngine> engine, AppMetrics& metrics, const std::string& host, int port)
    : engine_(std::move(engine)), metrics_(metrics), host_(host), port_(port) {
    setup_routes();
}

void HttpServer::setup_routes() {
    // 1. Statik Dosyalar (Omni-Studio)
    // Docker içinde /app/studio klasörünü kök (/) olarak sun
    auto ret = svr_.set_mount_point("/", "./studio");
    if (!ret) {
        spdlog::warn("⚠️ Could not mount ./studio directory. UI might not work.");
    }

    // 2. Health Check
    svr_.Get("/health", [this](const httplib::Request &, httplib::Response &res) {
        bool ready = engine_->is_ready();
        json response = {
            {"status", ready ? "healthy" : "unhealthy"},
            {"model_ready", ready},
            {"service", "sentiric-stt-whisper-service"},
            {"version", "2.0.0"}
        };
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(response.dump(), "application/json");
        res.status = ready ? 200 : 503;
    });

    // 3. Transcribe Endpoint (REST API)
    // Omni-Studio ve basit entegrasyonlar için
    svr_.Post("/v1/transcribe", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        metrics_.requests_total.Increment();
        
        if (!engine_->is_ready()) {
            res.status = 503;
            res.set_content(json{{"error", "Model not ready"}}.dump(), "application/json");
            return;
        }

        if (!req.has_file("file")) {
            res.status = 400;
            res.set_content(json{{"error", "No file uploaded"}}.dump(), "application/json");
            return;
        }

        const auto& file = req.get_file_value("file");
        spdlog::info("Received file upload: {} ({} bytes)", file.filename, file.content.size());

        try {
            // WAV parse et ve işle
            auto pcm16 = parse_wav(file.content);
            auto results = engine_->transcribe_pcm16(pcm16);

            // Sonucu birleştir
            std::string full_text;
            std::string lang = "unknown";
            
            for (const auto& r : results) {
                full_text += r.text;
                lang = r.language;
            }

            double duration = (double)pcm16.size() / 16000.0;
            metrics_.audio_seconds_processed_total.Increment(duration);

            json response = {
                {"text", full_text},
                {"language", lang},
                {"duration", duration}
            };

            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            spdlog::error("Transcription error: {}", e.what());
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
}

std::vector<int16_t> HttpServer::parse_wav(const std::string& bytes) {
    // Çok basit WAV header atlayıcı (Robust bir çözüm değil ama MVP için yeterli)
    // Standart WAV header 44 bytetır.
    if (bytes.size() < 44) return {};

    // Header'ı atla, kalanı kopyala
    // Not: Gelen verinin 16-bit PCM olduğunu varsayıyoruz.
    // Omni-Studio JS tarafında bunu garanti etmeliyiz.
    size_t data_size = bytes.size() - 44;
    // Çift sayıya yuvarla (2 byte = 1 sample)
    data_size = data_size - (data_size % 2);

    std::vector<int16_t> pcm(data_size / 2);
    std::memcpy(pcm.data(), bytes.data() + 44, data_size);
    
    return pcm;
}

void HttpServer::run() {
    spdlog::info("🌐 HTTP server (Studio & API) listening on {}:{}", host_, port_);
    svr_.listen(host_.c_str(), port_);
}

void HttpServer::stop() {
    if (svr_.is_running()) svr_.stop();
}
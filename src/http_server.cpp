#include "http_server.h"
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"
#include <prometheus/text_serializer.h>
#include <sstream>
#include <cstring>

using json = nlohmann::json;

// --- UTF-8 Sanitizer (Helper) ---
// Modelden gelen bozuk byte dizilerini temizler, JSON crash'ini önler.
std::string clean_utf8(const std::string& str) {
    std::string res;
    res.reserve(str.size());
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = str[i];
        int n;
        // UTF-8 byte uzunluğu tespiti
        if      (c < 0x80) n = 1;             // 0xxxxxxx
        else if ((c & 0xE0) == 0xC0) n = 2;   // 110xxxxx
        else if ((c & 0xF0) == 0xE0) n = 3;   // 1110xxxx
        else if ((c & 0xF8) == 0xF0) n = 4;   // 11110xxx
        else {
            // Geçersiz başlangıç byte'ı, atla
            i++; continue; 
        }

        if (i + n > str.size()) break; // String sonu hatası
        
        // Takip eden byte'ları kontrol et (10xxxxxx)
        bool valid = true;
        for (int j = 1; j < n; j++) {
            if ((str[i + j] & 0xC0) != 0x80) {
                valid = false;
                break;
            }
        }

        if (valid) {
            res.append(str, i, n);
            i += n;
        } else {
            i++; // Geçersiz dizi, sadece ilk byte'ı atla ve devam et
        }
    }
    return res;
}
// --------------------------------

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
            {"version", "2.1.1"}, // Patch version update
            {"api_compatibility", "openai-whisper"}
        };
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(response.dump(), "application/json");
        res.status = ready ? 200 : 503;
    });

    // 3. Transcribe Handler
    auto transcribe_handler = [this](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        metrics_.requests_total.Increment();
        
        if (!engine_->is_ready()) {
            res.status = 503;
            res.set_content(json{{"error", "Model not ready"}}.dump(), "application/json");
            return;
        }

        if (!req.has_file("file")) {
            res.status = 400;
            res.set_content(json{{"error", "No file uploaded."}}.dump(), "application/json");
            return;
        }

        const auto& file = req.get_file_value("file");
        
        std::string lang = "";
        if (req.has_file("language")) {
            lang = req.get_file_value("language").content;
        }

        spdlog::info("🎤 Processing Audio: {} ({} bytes) | Lang Request: {}", file.filename, file.content.size(), lang);

        try {
            auto start_time = std::chrono::steady_clock::now();
            
            auto pcm16 = parse_wav(file.content);
            auto results = engine_->transcribe_pcm16(pcm16, 16000, lang);

            auto end_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> processing_time = end_time - start_time;

            std::string full_text;
            std::string detected_lang = "unknown";
            json segments = json::array();
            
            for (const auto& r : results) {
                // DÜZELTME: UTF-8 Temizliği
                std::string safe_text = clean_utf8(r.text);
                full_text += safe_text;
                
                detected_lang = r.language;
                
                json words_json = json::array();
                for (const auto& t : r.tokens) {
                    words_json.push_back({
                        {"word", clean_utf8(t.text)}, // Token text'i de temizle
                        {"start", (double)t.t0 / 100.0},
                        {"end", (double)t.t1 / 100.0},
                        {"probability", t.p}
                    });
                }

                segments.push_back({
                    {"text", safe_text},
                    {"start", (double)r.t0 / 100.0},
                    {"end", (double)r.t1 / 100.0},
                    {"probability", r.prob},
                    {"words", words_json}
                });
            }

            double duration = (double)pcm16.size() / 16000.0;
            metrics_.audio_seconds_processed_total.Increment(duration);
            metrics_.request_latency.Observe(processing_time.count());

            json response = {
                {"text", full_text},
                {"language", detected_lang},
                {"duration", duration},
                {"segments", segments},
                {"meta", {
                    {"processing_time", processing_time.count()},
                    {"rtf", processing_time.count() / (duration > 0 ? duration : 1.0)},
                    {"device", "native-cpp-v2.1.1"}
                }}
            };

            spdlog::info("✅ Transcription Done. Audio: {:.2f}s, Proc: {:.2f}s, RTF: {:.2f}x", 
                         duration, processing_time.count(), duration / processing_time.count());

            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            spdlog::error("Transcription error: {}", e.what());
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    };

    svr_.Post("/v1/transcribe", transcribe_handler);
    svr_.Post("/v1/audio/transcriptions", transcribe_handler);
}

std::vector<int16_t> HttpServer::parse_wav(const std::string& bytes) {
    if (bytes.size() < 44) return {};
    size_t data_size = bytes.size() - 44;
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
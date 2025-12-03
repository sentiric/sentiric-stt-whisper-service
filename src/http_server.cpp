#include "http_server.h"
#include "utils.h"
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"
#include <prometheus/text_serializer.h>
#include <sstream>
#include <algorithm>

using json = nlohmann::json;
using namespace sentiric::utils;

MetricsServer::MetricsServer(const std::string& host, int port, prometheus::Registry& registry)
    : host_(host), port_(port), registry_(registry) {
    
    // YENİ: CORS Headerları eklendi. Omni-Studio (15030) buradan (15032) veri çekebilsin.
    svr_.Get("/metrics", [this](const httplib::Request &, httplib::Response &res) {
        prometheus::TextSerializer serializer;
        auto collected_metrics = this->registry_.Collect();
        std::stringstream ss; serializer.Serialize(ss, collected_metrics);
        
        // NOT: Production ortamında '*' yerine belirli domainlerin (örn: dashboard domaini)
        // verilmesi güvenlik açısından daha doğrudur. Standalone kullanım kolaylığı için '*' bırakılmıştır.
        res.set_header("Access-Control-Allow-Origin", "*"); 
        res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        
        res.set_content(ss.str(), "text/plain; version=0.0.4");
    });

    // Options isteği için de (Preflight)
    svr_.Options("/metrics", [](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });
}
void MetricsServer::run() { spdlog::info("📊 Metrics server listening on {}:{}", host_, port_); svr_.listen(host_.c_str(), port_); }
void MetricsServer::stop() { if (svr_.is_running()) svr_.stop(); }

HttpServer::HttpServer(std::shared_ptr<SttEngine> engine, AppMetrics& metrics, const std::string& host, int port)
    : engine_(std::move(engine)), metrics_(metrics), host_(host), port_(port) { setup_routes(); }

void HttpServer::setup_routes() {
    auto ret = svr_.set_mount_point("/", "./studio");
    if (!ret) spdlog::warn("⚠️ Could not mount ./studio directory.");
    svr_.Get("/health", [this](const httplib::Request &, httplib::Response &res) {
        bool ready = engine_->is_ready();
        // VERSİYON GÜNCELLENDİ: 2.5.0 -> 2.5.1
        json response = { {"status", ready ? "healthy" : "unhealthy"}, {"model_ready", ready}, {"service", "sentiric-stt-whisper-service"}, {"version", "2.5.1"}, {"api_compatibility", "openai-whisper"} };
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(response.dump(), "application/json"); res.status = ready ? 200 : 503;
    });

    auto transcribe_handler = [this](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*"); metrics_.requests_total.Increment();
        if (!engine_->is_ready()) { res.status = 503; res.set_content(json{{"error", "Model not ready"}}.dump(), "application/json"); return; }
        if (!req.has_file("file")) { res.status = 400; res.set_content(json{{"error", "No file uploaded."}}.dump(), "application/json"); return; }
        const auto& file = req.get_file_value("file");
        RequestOptions opts;
        
        // --- Standart Parametreler ---
        if (req.has_file("language")) opts.language = req.get_file_value("language").content;
        if (req.has_file("prompt")) opts.prompt = req.get_file_value("prompt").content;
        if (req.has_file("temperature")) { try { opts.temperature = std::stof(req.get_file_value("temperature").content); } catch(...) {} }
        if (req.has_file("beam_size")) { try { opts.beam_size = std::stoi(req.get_file_value("beam_size").content); } catch(...) {} }
        if (req.has_file("translate")) { std::string val = req.get_file_value("translate").content; opts.translate = (val == "true" || val == "1"); }
        if (req.has_file("diarization")) { std::string val = req.get_file_value("diarization").content; opts.enable_diarization = (val == "true" || val == "1"); }
        
        // --- YENİ: Gelişmiş DSP Parametreleri ---
        if (req.has_file("prosody_lpf_alpha")) { 
            try { opts.prosody_opts.lpf_alpha = std::stof(req.get_file_value("prosody_lpf_alpha").content); } catch(...) {} 
        }
        if (req.has_file("prosody_pitch_gate")) { 
            try { opts.prosody_opts.gender_threshold = std::stof(req.get_file_value("prosody_pitch_gate").content); } catch(...) {} 
        }

        spdlog::info("🎤 Processing: {}b | Lang: {} | LPF: {:.3f}", file.content.size(), opts.language, opts.prosody_opts.lpf_alpha);
        
        try {
            auto start_time = std::chrono::steady_clock::now();
            DecodedAudio audio = parse_wav_robust(file.content);
            if (audio.pcm_data.empty()) throw std::runtime_error("Parsed WAV data is empty.");
            
            // --- Abort Callback Lambda ---
            // HTTP sunucusu blocking olduğu için şu anlık abort callback vermiyoruz.
            // İleride async yapıya geçilirse buraya eklenebilir.
            
            auto results = engine_->transcribe_pcm16(audio.pcm_data, audio.sample_rate, opts);
            auto end_time = std::chrono::steady_clock::now(); std::chrono::duration<double> processing_time = end_time - start_time;
            
            std::string full_text; std::string detected_lang = "unknown"; json segments = json::array();
            int total_tokens = 0; // Metrik için

            for (const auto& r : results) {
                std::string safe_text = clean_utf8(r.text); full_text += safe_text; detected_lang = r.language;
                total_tokens += r.token_count;
                
                json words_json = json::array();
                for (const auto& t : r.tokens) words_json.push_back({ {"word", clean_utf8(t.text)}, {"start", (double)t.t0 / 100.0}, {"end", (double)t.t1 / 100.0}, {"probability", t.p} });
                const auto& aff = r.affective;
                segments.push_back({
                    {"text", safe_text}, {"start", (double)r.t0 / 100.0}, {"end", (double)r.t1 / 100.0}, {"probability", r.prob},
                    {"speaker_turn_next", r.speaker_turn_next}, 
                    {"speaker_id", r.speaker_id},
                    {"gender", aff.gender_proxy}, {"emotion", aff.emotion_proxy},
                    {"arousal", aff.arousal}, {"valence", aff.valence},
                    {"pitch_mean", aff.pitch_mean}, {"pitch_std", aff.pitch_std},
                    {"energy_mean", aff.energy_mean}, {"energy_std", aff.energy_std},
                    {"spectral_centroid", aff.spectral_centroid}, {"zero_crossing_rate", aff.zero_crossing_rate},
                    {"speaker_vec", aff.speaker_vec}, {"words", words_json}
                });
            }
            double duration = static_cast<double>(audio.pcm_data.size()) / static_cast<double>(audio.sample_rate);
            
            // Metrik Güncellemeleri
            metrics_.audio_seconds_processed_total.Increment(duration); 
            metrics_.request_latency.Observe(processing_time.count());
            metrics_.tokens_generated_total.Increment(total_tokens);

            json response = {
                {"text", full_text}, {"language", detected_lang}, {"duration", duration}, {"segments", segments},
                {"meta", { {"processing_time", processing_time.count()}, {"rtf", processing_time.count() / (duration > 0 ? duration : 1.0)}, {"input_sr", audio.sample_rate}, {"input_channels", audio.channels}, {"tokens", total_tokens} } }
            };
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            spdlog::error("Transcription error: {}", e.what());
            res.status = 500; res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    };
    svr_.Post("/v1/transcribe", transcribe_handler);
    svr_.Post("/v1/audio/transcriptions", transcribe_handler);
}

void HttpServer::run() { spdlog::info("🌐 HTTP server (Studio & API) listening on {}:{}", host_, port_); svr_.listen(host_.c_str(), port_); }
void HttpServer::stop() { if (svr_.is_running()) svr_.stop(); }
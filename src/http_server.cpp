#include "http_server.h"
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"
#include <prometheus/text_serializer.h>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <iomanip>

using json = nlohmann::json;

std::string clean_utf8(const std::string& str) {
    std::string res;
    res.reserve(str.size());
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = str[i];
        int n;
        if      (c < 0x80) n = 1;             
        else if ((c & 0xE0) == 0xC0) n = 2;   
        else if ((c & 0xF0) == 0xE0) n = 3;   
        else if ((c & 0xF8) == 0xF0) n = 4;   
        else { i++; continue; }
        if (i + n > str.size()) break; 
        bool valid = true;
        for (int j = 1; j < n; j++) {
            if ((str[i + j] & 0xC0) != 0x80) { valid = false; break; }
        }
        if (valid) { res.append(str, i, n); i += n; } else { i++; }
    }
    return res;
}

std::string hex_dump(const void* data, size_t size) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i) {
        ss << std::setw(2) << static_cast<int>(p[i]) << " ";
    }
    return ss.str();
}

// --- MetricsServer ---
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

// --- HttpServer ---
HttpServer::HttpServer(std::shared_ptr<SttEngine> engine, AppMetrics& metrics, const std::string& host, int port)
    : engine_(std::move(engine)), metrics_(metrics), host_(host), port_(port) {
    setup_routes();
}

void HttpServer::setup_routes() {
    auto ret = svr_.set_mount_point("/", "./studio");
    if (!ret) spdlog::warn("⚠️ Could not mount ./studio directory.");

    svr_.Get("/health", [this](const httplib::Request &, httplib::Response &res) {
        bool ready = engine_->is_ready();
        json response = {
            {"status", ready ? "healthy" : "unhealthy"},
            {"model_ready", ready},
            {"service", "sentiric-stt-whisper-service"},
            {"version", "2.2.4"}, // Patch: Wav Self-Healing
            {"api_compatibility", "openai-whisper"}
        };
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(response.dump(), "application/json");
        res.status = ready ? 200 : 503;
    });

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
        
        RequestOptions opts;
        if (req.has_file("language")) opts.language = req.get_file_value("language").content;
        if (req.has_file("prompt")) opts.prompt = req.get_file_value("prompt").content;
        
        if (req.has_file("temperature")) {
            try { opts.temperature = std::stof(req.get_file_value("temperature").content); } catch(...) {}
        }
        if (req.has_file("beam_size")) {
            try { opts.beam_size = std::stoi(req.get_file_value("beam_size").content); } catch(...) {}
        }
        if (req.has_file("translate")) {
            std::string val = req.get_file_value("translate").content;
            opts.translate = (val == "true" || val == "1");
        }
        if (req.has_file("diarization")) {
            std::string val = req.get_file_value("diarization").content;
            opts.enable_diarization = (val == "true" || val == "1");
        }

        spdlog::info("🎤 Processing: {}b | Lang: {} | Temp: {:.1f}", file.content.size(), opts.language, opts.temperature);

        try {
            auto start_time = std::chrono::steady_clock::now();
            
            DecodedAudio audio = parse_wav_robust(file.content);
            
            if (audio.pcm_data.empty()) throw std::runtime_error("Parsed WAV data is empty.");

            auto results = engine_->transcribe_pcm16(audio.pcm_data, audio.sample_rate, opts);
            
            auto end_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> processing_time = end_time - start_time;

            std::string full_text;
            std::string detected_lang = "unknown";
            json segments = json::array();
            
            for (const auto& r : results) {
                std::string safe_text = clean_utf8(r.text);
                full_text += safe_text;
                detected_lang = r.language;
                
                json words_json = json::array();
                for (const auto& t : r.tokens) {
                    words_json.push_back({
                        {"word", clean_utf8(t.text)},
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
                    {"speaker_turn_next", r.speaker_turn_next},
                    {"words", words_json}
                });
            }

            double duration = (double)audio.pcm_data.size() / (double)audio.sample_rate;
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
                    {"input_sr", audio.sample_rate},
                    {"input_channels", audio.channels}
                }}
            };

            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            spdlog::error("Transcription error: {}", e.what());
            if (file.content.size() > 0) {
                size_t dump_size = std::min((size_t)64, file.content.size());
                spdlog::error("WAV Header Dump (First {} bytes): {}", dump_size, hex_dump(file.content.data(), dump_size));
            }
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    };

    svr_.Post("/v1/transcribe", transcribe_handler);
    svr_.Post("/v1/audio/transcriptions", transcribe_handler);
}

DecodedAudio HttpServer::parse_wav_robust(const std::string& bytes) {
    DecodedAudio result;
    if (bytes.size() < 44) throw std::runtime_error("WAV too small (<44 bytes)");

    const uint8_t* data = reinterpret_cast<const uint8_t*>(bytes.data());
    size_t ptr = 0;

    if (std::memcmp(data + ptr, "RIFF", 4) != 0) throw std::runtime_error("Invalid WAV: No RIFF");
    ptr += 8; 
    if (std::memcmp(data + ptr, "WAVE", 4) != 0) throw std::runtime_error("Invalid WAV: No WAVE");
    ptr += 4;

    const uint8_t* pcm_start = nullptr;
    size_t pcm_size_bytes = 0;
    int16_t bits_per_sample = 0;
    bool fmt_found = false;

    while (ptr + 8 < bytes.size()) {
        char chunk_id[5] = {0};
        std::memcpy(chunk_id, data + ptr, 4);
        ptr += 4;

        uint32_t chunk_size;
        std::memcpy(&chunk_size, data + ptr, 4);
        ptr += 4;

        if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
            // --- SELF-HEALING LOGIC ---
            if (chunk_size == 0) {
                spdlog::warn("⚠️ Malformed WAV detected: 'fmt ' chunk size is 0. Attempting repair (Assuming standard PCM 16).");
                chunk_size = 16; 
            }
            // --------------------------

            if (chunk_size < 16) {
                throw std::runtime_error("Invalid fmt chunk size: " + std::to_string(chunk_size));
            }
            
            uint16_t format_tag;
            std::memcpy(&format_tag, data + ptr, 2);
            if (format_tag != 1 && format_tag != 0xFFFE) {
                throw std::runtime_error("Unsupported format_tag: " + std::to_string(format_tag));
            }

            std::memcpy(&result.channels, data + ptr + 2, 2);
            std::memcpy(&result.sample_rate, data + ptr + 4, 4);
            std::memcpy(&bits_per_sample, data + ptr + 14, 2);
            
            fmt_found = true;
            ptr += chunk_size;
        } 
        else if (std::memcmp(chunk_id, "data", 4) == 0) {
            if (!fmt_found) throw std::runtime_error("Found data chunk before fmt chunk");
            
            pcm_start = data + ptr;
            pcm_size_bytes = chunk_size;
            break; 
        } 
        else {
            ptr += chunk_size;
        }

        if (chunk_size % 2 != 0) ptr++;
    }

    if (!pcm_start || pcm_size_bytes == 0) throw std::runtime_error("No 'data' chunk found");

    if (bits_per_sample != 16) throw std::runtime_error("Unsupported bit depth: " + std::to_string(bits_per_sample));

    size_t remaining = bytes.size() - (pcm_start - data);
    if (pcm_size_bytes > remaining) {
        spdlog::warn("WAV data chunk size ({}) > remaining bytes ({})! Truncating.", pcm_size_bytes, remaining);
        pcm_size_bytes = remaining;
    }

    size_t num_samples = pcm_size_bytes / 2;
    const int16_t* raw_samples = reinterpret_cast<const int16_t*>(pcm_start);

    if (result.channels == 1) {
        result.pcm_data.assign(raw_samples, raw_samples + num_samples);
    } 
    else if (result.channels == 2) {
        size_t frames = num_samples / 2;
        result.pcm_data.resize(frames);
        for (size_t i = 0; i < frames; ++i) {
            int32_t mixed = (int32_t)raw_samples[i*2] + (int32_t)raw_samples[i*2 + 1];
            result.pcm_data[i] = static_cast<int16_t>(mixed / 2);
        }
    } 
    else {
        size_t frames = num_samples / result.channels;
        result.pcm_data.resize(frames);
        for (size_t i = 0; i < frames; ++i) {
            result.pcm_data[i] = raw_samples[i * result.channels];
        }
    }

    return result;
}

void HttpServer::run() {
    spdlog::info("🌐 HTTP server (Studio & API) listening on {}:{}", host_, port_);
    svr_.listen(host_.c_str(), port_);
}

void HttpServer::stop() {
    if (svr_.is_running()) svr_.stop();
}
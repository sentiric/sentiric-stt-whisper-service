#include "http_server.h"
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"
#include <prometheus/text_serializer.h>
#include <sstream>
#include <cstring>
#include <algorithm>

using json = nlohmann::json;

// Helper: UTF-8 Temizliği
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

// --- MetricsServer Implementation ---

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

// --- HttpServer Implementation ---

HttpServer::HttpServer(std::shared_ptr<SttEngine> engine, AppMetrics& metrics, const std::string& host, int port)
    : engine_(std::move(engine)), metrics_(metrics), host_(host), port_(port) {
    setup_routes();
}

void HttpServer::setup_routes() {
    auto ret = svr_.set_mount_point("/", "./studio");
    if (!ret) {
        spdlog::warn("⚠️ Could not mount ./studio directory. UI might not work.");
    }

    svr_.Get("/health", [this](const httplib::Request &, httplib::Response &res) {
        bool ready = engine_->is_ready();
        json response = {
            {"status", ready ? "healthy" : "unhealthy"},
            {"model_ready", ready},
            {"service", "sentiric-stt-whisper-service"},
            {"version", "2.2.1"}, // Patch Bump for WAV Fix
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
        std::string lang = "";
        if (req.has_file("language")) {
            lang = req.get_file_value("language").content;
        }

        std::string prompt = "";
        if (req.has_file("prompt")) {
            prompt = req.get_file_value("prompt").content;
        }

        spdlog::info("🎤 Processing Audio: {} ({} bytes) | Lang: {} | Prompt: {}", 
            file.filename, file.content.size(), lang, prompt);

        try {
            auto start_time = std::chrono::steady_clock::now();
            
            // ADIM 1: Sağlam WAV Ayrıştırma (Parse)
            DecodedAudio audio = parse_wav_robust(file.content);
            
            if (audio.pcm_data.empty()) {
                throw std::runtime_error("Invalid or empty WAV file.");
            }

            spdlog::debug("Audio Decoded: Rate={}, Channels={}, Samples={}", 
                audio.sample_rate, audio.channels, audio.pcm_data.size());

            // ADIM 2: Transkripsiyon (Doğru Sample Rate ile)
            auto results = engine_->transcribe_pcm16(
                audio.pcm_data, 
                audio.sample_rate, // ARTIK DOĞRU RATE GİDİYOR
                lang, 
                prompt
            );
            
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

            // Süre hesaplaması (Orijinal SR üzerinden değil, normalize edilmiş 16kHz üzerinden de değil, giriş süresi)
            // SttEngine resample yapıyor ama giriş boyutu / giriş rate gerçek süreyi verir.
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
                    {"device", "native-cpp-v2.2.0"},
                    {"input_sr", audio.sample_rate},
                    {"input_channels", audio.channels}
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

// RIFF/WAVE formatını chunk-base okuyan sağlam fonksiyon
DecodedAudio HttpServer::parse_wav_robust(const std::string& bytes) {
    DecodedAudio result;
    if (bytes.size() < 44) return result; // Header bile yok

    const uint8_t* data = reinterpret_cast<const uint8_t*>(bytes.data());
    size_t ptr = 0;

    // 1. RIFF Header Kontrolü
    if (memcmp(data + ptr, "RIFF", 4) != 0) throw std::runtime_error("Invalid WAV: No RIFF header");
    ptr += 8; // "RIFF" + Size (4 byte) atla
    if (memcmp(data + ptr, "WAVE", 4) != 0) throw std::runtime_error("Invalid WAV: No WAVE header");
    ptr += 4;

    // 2. Chunk Iterasyonu (fmt, data, list, etc.)
    int16_t bits_per_sample = 0;
    const uint8_t* pcm_start = nullptr;
    size_t pcm_size_bytes = 0;

    while (ptr + 8 < bytes.size()) {
        char chunk_id[5] = {0};
        memcpy(chunk_id, data + ptr, 4);
        ptr += 4;

        uint32_t chunk_size;
        memcpy(&chunk_size, data + ptr, 4);
        ptr += 4;

        if (strcmp(chunk_id, "fmt ") == 0) {
            if (chunk_size < 16) throw std::runtime_error("Invalid fmt chunk");
            uint16_t format_tag;
            memcpy(&format_tag, data + ptr, 2);
            if (format_tag != 1 && format_tag != 0xFFFE) { // 1=PCM, 0xFFFE=Extensible
                throw std::runtime_error("Unsupported WAV format (Non-PCM)");
            }

            uint16_t channels;
            memcpy(&channels, data + ptr + 2, 2);
            result.channels = channels;

            uint32_t sample_rate;
            memcpy(&sample_rate, data + ptr + 4, 4);
            result.sample_rate = sample_rate;

            memcpy(&bits_per_sample, data + ptr + 14, 2);
            
            ptr += chunk_size; // Chunk içeriğini atla (okuduk zaten)
        } 
        else if (strcmp(chunk_id, "data") == 0) {
            pcm_start = data + ptr;
            pcm_size_bytes = chunk_size;
            // Data chunk'ını bulduk, diğerlerini okumaya gerek yok mu? 
            // Bazen data'dan sonra da metadata olabilir ama PCM'i bulduysak yeterli.
            ptr += chunk_size;
            break; 
        } 
        else {
            // Bilinmeyen chunk (LIST, INFO, id3 vs), atla
            ptr += chunk_size;
        }

        // Chunk size tek sayı ise padding byte vardır
        if (chunk_size % 2 != 0) ptr++;
    }

    if (!pcm_start || pcm_size_bytes == 0) {
        throw std::runtime_error("No 'data' chunk found in WAV");
    }

    // 3. PCM Verisini Çıkar ve Mono'ya Çevir
    if (bits_per_sample != 16) {
        // Şimdilik sadece 16-bit destekliyoruz (Whisper standardı)
        // 24-bit veya 32-bit float için converter gerekir, burada basit tutuyoruz.
        throw std::runtime_error("Unsupported bit depth: " + std::to_string(bits_per_sample) + " (Only 16-bit PCM supported)");
    }

    // Byte -> Int16
    size_t num_samples = pcm_size_bytes / 2;
    const int16_t* raw_samples = reinterpret_cast<const int16_t*>(pcm_start);

    if (result.channels == 1) {
        // Zaten Mono, direkt kopyala
        result.pcm_data.assign(raw_samples, raw_samples + num_samples);
    } 
    else if (result.channels == 2) {
        // Stereo -> Mono Downmix (L+R)/2
        // Interleaved: L, R, L, R...
        size_t frames = num_samples / 2;
        result.pcm_data.resize(frames);
        for (size_t i = 0; i < frames; ++i) {
            int32_t mixed = (int32_t)raw_samples[i*2] + (int32_t)raw_samples[i*2 + 1];
            result.pcm_data[i] = static_cast<int16_t>(mixed / 2);
        }
    } 
    else {
        // Çok kanallı ise sadece ilk kanalı al (Basit çözüm)
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
#include "config.h"
#include "stt_engine.h"
#include "grpc_server.h"
#include "http_server.h"
#include <thread>
#include <memory>
#include <csignal>
#include <future>
#include <fstream>
#include <sstream>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include "model_manager.h" // EKLENDİ

#include "spdlog/sinks/stdout_color_sinks.h" 
// -----------------------------------------------

namespace {
    std::promise<void> shutdown_promise;
}

void signal_handler(int signal) {
    spdlog::warn("Signal {} received. Initiating graceful shutdown...", signal);
    try { shutdown_promise.set_value(); } catch (...) {}
}

void whisper_log_cb(ggml_log_level level, const char * text, void * user_data) {
    (void)user_data;
    std::string msg(text);
    if (!msg.empty() && msg.back() == '\n') msg.pop_back();
    
    switch(level) {
        case GGML_LOG_LEVEL_ERROR: spdlog::error("[whisper] {}", msg); break;
        case GGML_LOG_LEVEL_WARN: spdlog::warn("[whisper] {}", msg); break;
        case GGML_LOG_LEVEL_INFO: spdlog::info("[whisper] {}", msg); break;
        default: spdlog::debug("[whisper] {}", msg); break;
    }
}

std::string read_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) throw std::runtime_error("File not found: " + filepath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}


int main() {
    // Loglama Başlat
    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    
    whisper_log_set(whisper_log_cb, nullptr);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    auto settings = load_settings();
    spdlog::set_level(spdlog::level::from_str(settings.log_level));
    
    spdlog::info("🚀 Sentiric STT Whisper Service (C++) Starting...");
    spdlog::info("Config: Model={}, Threads={}, Device={}", settings.model_filename, settings.n_threads, 
        #ifdef GGML_USE_CUDA 
        "CUDA" 
        #else 
        "CPU" 
        #endif
    );

    auto registry = std::make_shared<prometheus::Registry>();
    auto& req_total = prometheus::BuildCounter().Name("stt_requests_total").Register(*registry).Add({});
    
    // --- DÜZELTME 2: Histogram bucket tanımlaması ---
    prometheus::Histogram::BucketBoundaries buckets{0.1, 0.5, 1.0, 5.0, 10.0};
    auto& req_latency = prometheus::BuildHistogram()
                            .Name("stt_request_latency_seconds")
                            .Register(*registry)
                            .Add({}, buckets); // Açıkça bucket boundaries nesnesini geçiriyoruz
    // -----------------------------------------------

    auto& audio_sec = prometheus::BuildCounter().Name("stt_audio_seconds_processed_total").Register(*registry).Add({});
    
    AppMetrics metrics = {req_total, req_latency, audio_sec};

    try {
        // --- YENİ: Model Manager Entegrasyonu ---
        // Servis başlamadan önce modelin hazır olduğundan emin ol
        std::string model_path = ModelManager::ensure_model(settings);
        // Engine artık sadece path'i bilse yeterli olur ama config'den alıyor.
        // Config zaten doğru path'i gösteriyor olmalı.
        // ----------------------------------------

        auto engine = std::make_shared<SttEngine>(settings);

        grpc::EnableDefaultHealthCheckService(true);
        
        GrpcServer grpc_service(engine, metrics);
        grpc::ServerBuilder builder;
        
        std::string grpc_addr = settings.host + ":" + std::to_string(settings.grpc_port);
        
        if (settings.grpc_ca_path.empty()) {
            builder.AddListeningPort(grpc_addr, grpc::InsecureServerCredentials());
            spdlog::warn("gRPC listening on {} (INSECURE)", grpc_addr);
        } else {
            std::string root = read_file(settings.grpc_ca_path);
            std::string cert = read_file(settings.grpc_cert_path);
            std::string key = read_file(settings.grpc_key_path);
            
            grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp = {key, cert};
            grpc::SslServerCredentialsOptions ssl_opts;
            ssl_opts.pem_root_certs = root;
            ssl_opts.pem_key_cert_pairs.push_back(pkcp);
            
            builder.AddListeningPort(grpc_addr, grpc::SslServerCredentials(ssl_opts));
            spdlog::info("gRPC listening on {} (mTLS Enabled)", grpc_addr);
        }

        builder.RegisterService(&grpc_service);
        std::unique_ptr<grpc::Server> grpc_server = builder.BuildAndStart();

        HttpServer http_server(engine, settings.host, settings.http_port);
        MetricsServer metrics_server(settings.host, settings.http_port + 100, *registry);

        std::thread http_thread([&](){ http_server.run(); });
        std::thread metrics_thread([&](){ metrics_server.run(); });
        
        spdlog::info("✅ Service Ready!");

        shutdown_promise.get_future().wait();
        
        spdlog::info("Stopping servers...");
        grpc_server->Shutdown();
        http_server.stop();
        metrics_server.stop();
        
        if(http_thread.joinable()) http_thread.join();
        if(metrics_thread.joinable()) metrics_thread.join();

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
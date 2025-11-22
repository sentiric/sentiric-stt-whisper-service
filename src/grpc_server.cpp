#include "grpc_server.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <vector>

GrpcServer::GrpcServer(std::shared_ptr<SttEngine> engine, AppMetrics& metrics)
    : engine_(std::move(engine)), metrics_(metrics) {}

grpc::Status GrpcServer::WhisperTranscribe(
    grpc::ServerContext* context,
    const sentiric::stt::v1::WhisperTranscribeRequest* request,
    sentiric::stt::v1::WhisperTranscribeResponse* response) {
    
    (void)context; // Unused parameter warning'i önle
    metrics_.requests_total.Increment();
    auto start_time = std::chrono::steady_clock::now();

    if (!engine_->is_ready()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model not ready");
    }

    // Gelen bytes -> int16_t dönüşümü
    // Varsayım: Gelen veri 16-bit PCM
    const std::string& audio_data = request->audio_data();
    if (audio_data.size() % 2 != 0) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Audio data length must be even (16-bit PCM)");
    }

    // Ses verisini kopyala
    std::vector<int16_t> pcm16(audio_data.size() / 2);
    std::memcpy(pcm16.data(), audio_data.data(), audio_data.size());

    // --- FIX: İSTEKTEN DİLİ AL ---
    std::string req_lang = "";
    if (request->has_language()) {
        req_lang = request->language();
    }
    // -----------------------------

    // Motoru çağır (Dil parametresiyle)
    auto results = engine_->transcribe_pcm16(pcm16, 16000, req_lang);

    // Sonucu birleştir
    std::string full_text;
    float avg_prob = 0.0f;
    
    for (const auto& res : results) {
        full_text += res.text;
        avg_prob += res.prob;
    }
    if (!results.empty()) avg_prob /= results.size();

    response->set_transcription(full_text);
    response->set_language(results.empty() ? "unknown" : results[0].language);
    response->set_language_probability(avg_prob);
    
    // Süre hesapla
    double duration_sec = (double)pcm16.size() / 16000.0; // 16kHz varsayımı
    response->set_duration(duration_sec);

    // Metrikleri güncelle
    metrics_.audio_seconds_processed_total.Increment(duration_sec);
    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> latency = end_time - start_time;
    metrics_.request_latency.Observe(latency.count());

    spdlog::info("Processed audio: {:.2f}s, Latency: {:.2f}s, Text: {}", duration_sec, latency.count(), full_text);

    return grpc::Status::OK;
}

grpc::Status GrpcServer::WhisperTranscribeStream(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<sentiric::stt::v1::WhisperTranscribeStreamResponse, sentiric::stt::v1::WhisperTranscribeStreamRequest>* stream) {
    
    metrics_.requests_total.Increment();
    auto start_time = std::chrono::steady_clock::now();
    
    if (!engine_->is_ready()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model not ready");
    }

    sentiric::stt::v1::WhisperTranscribeStreamRequest request;
    std::vector<int16_t> full_pcm_buffer;

    spdlog::info("Starting streaming transcription...");

    // 1. ADIM: Tüm veriyi al (Buffered Streaming)
    while (stream->Read(&request)) {
        const std::string& chunk = request.audio_chunk();
        if (chunk.empty()) continue;

        size_t current_size = full_pcm_buffer.size();
        size_t chunk_samples = chunk.size() / 2;
        full_pcm_buffer.resize(current_size + chunk_samples);
        std::memcpy(full_pcm_buffer.data() + current_size, chunk.data(), chunk.size());
    }

    if (context->IsCancelled()) {
        spdlog::warn("Stream cancelled by client");
        return grpc::Status::CANCELLED;
    }

    // 2. ADIM: Toplu İşle
    auto results = engine_->transcribe_pcm16(full_pcm_buffer);

    // 3. ADIM: Sonuçları Parça Parça Gönder
    for (size_t i = 0; i < results.size(); ++i) {
        sentiric::stt::v1::WhisperTranscribeStreamResponse response;
        response.set_transcription(results[i].text);
        response.set_is_final(i == results.size() - 1); // Son parça mı?
        stream->Write(response);
    }

    // Metrik
    double duration_sec = (double)full_pcm_buffer.size() / 16000.0;
    metrics_.audio_seconds_processed_total.Increment(duration_sec);
    
    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> latency = end_time - start_time;
    metrics_.request_latency.Observe(latency.count());

    spdlog::info("Stream finished. Total Audio: {:.2f}s, Latency: {:.2f}s", duration_sec, latency.count());

    return grpc::Status::OK;
}
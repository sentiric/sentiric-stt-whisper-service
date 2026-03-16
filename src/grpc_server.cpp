// Dosya: src/grpc_server.cpp
#include "grpc_server.h"
#include "utils.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <vector>
#include <cstring>
#include <functional>

using namespace sentiric::utils;

GrpcServer::GrpcServer(std::shared_ptr<SttEngine> engine, AppMetrics& metrics)
    : engine_(std::move(engine)), metrics_(metrics) {}

grpc::Status GrpcServer::WhisperTranscribe(
    grpc::ServerContext* context,
    const sentiric::stt::v1::WhisperTranscribeRequest* request,
    sentiric::stt::v1::WhisperTranscribeResponse* response)
{
    // [ARCH-COMPLIANCE] constraints.yaml'ın gerektirdiği şekilde trace_id context propagation eklendi
    std::string trace_id = "unknown";
    auto metadata = context->client_metadata();
    auto it = metadata.find("x-trace-id");
    if (it != metadata.end()) {
        trace_id = std::string(it->second.data(), it->second.length());
    }

    metrics_.requests_total.Increment();
    auto start_time = std::chrono::steady_clock::now();
    spdlog::info("[trace_id: {}] 📡 Unary gRPC Transcribe requested.", trace_id);
    
    if (!engine_->is_ready()) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model not ready");
    
    DecodedAudio audio;
    try { audio = parse_wav_robust(request->audio_data()); } 
    catch (...) { return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid audio"); }
    
    RequestOptions options; 
    if (request->has_language()) options.language = request->language();
    
    auto results = engine_->transcribe_pcm16(audio.pcm_data, audio.sample_rate, options);
    
    if (!results.empty()) {
        response->set_transcription(results[0].text);
        response->set_language(results[0].language);
    }
    return grpc::Status::OK;
}

grpc::Status GrpcServer::WhisperTranscribeStream(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<sentiric::stt::v1::WhisperTranscribeStreamResponse, sentiric::stt::v1::WhisperTranscribeStreamRequest>* stream)
{
    // [ARCH-COMPLIANCE] constraints.yaml'ın gerektirdiği şekilde trace_id context propagation eklendi
    std::string trace_id = "unknown";
    auto metadata = context->client_metadata();
    auto it = metadata.find("x-trace-id");
    if (it != metadata.end()) {
        trace_id = std::string(it->second.data(), it->second.length());
    }

    metrics_.requests_total.Increment();
    spdlog::info("[trace_id: {}] 📡 New gRPC Stream Connection started.", trace_id);
    
    if (!engine_->is_ready()) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model not ready");
    
    sentiric::stt::v1::WhisperTranscribeStreamRequest request;
    std::vector<int16_t> buffer;
    
    const size_t MAX_BUFFER_SIZE = 16000 * 30; 
    const size_t VAD_WINDOW = 16000 * 1;       
    
    bool is_first_chunk = true; 
    bool is_wav_container = false; 
    size_t wav_header_skip = 0;
    
    while (stream->Read(&request)) {
        if (context->IsCancelled()) return grpc::Status::CANCELLED;

        const std::string& chunk = request.audio_chunk();
        if (chunk.empty()) continue;

        const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(chunk.data());
        size_t data_len = chunk.size();
        
        if (is_first_chunk) {
            if (has_wav_header(chunk)) {
                is_wav_container = true; 
                if (chunk.size() > 44) wav_header_skip = 44;
            }
            is_first_chunk = false;
        }
        
        if (is_wav_container && wav_header_skip > 0) {
            if (data_len >= wav_header_skip) { 
                data_ptr += wav_header_skip; data_len -= wav_header_skip; wav_header_skip = 0; 
            } else { 
                wav_header_skip -= data_len; data_len = 0; 
            }
        }

        if (data_len > 0) {
            size_t samples = data_len / 2;
            size_t current_size = buffer.size();
            buffer.resize(current_size + samples);
            std::memcpy(buffer.data() + current_size, data_ptr, samples * 2);
        }

        if (buffer.size() > 24000) {
             spdlog::info("[trace_id: {}] ⚡ Processing buffered chunk ({} samples)...", trace_id, buffer.size());
             
             RequestOptions options;
             SttEngine::PerformanceMetrics perf;
             
             try {
                 auto results = engine_->transcribe_pcm16(buffer, 16000, options, &perf);
                 
                 for (const auto& res : results) {
                     if (!res.text.empty()) {
                         sentiric::stt::v1::WhisperTranscribeStreamResponse response;
                         response.set_transcription(res.text);
                         response.set_is_final(true);
                         
                         const auto& aff = res.affective;
                         response.set_gender_proxy(aff.gender_proxy);
                         response.set_emotion_proxy(aff.emotion_proxy);
                         
                         if (!stream->Write(response)) {
                             spdlog::warn("[trace_id: {}] Failed to write to stream, client likely disconnected.", trace_id);
                             return grpc::Status::OK; 
                         }
                         spdlog::info("[trace_id: {}] 📤 Sent Transcript: '{}'", trace_id, res.text);
                     }
                 }
                 
                 buffer.clear();
                 
             } catch (const std::exception& e) {
                 spdlog::error("[trace_id: {}] Streaming transcribe error: {}", trace_id, e.what());
             }
        }
    }
    
    if (!buffer.empty()) {
         spdlog::info("[trace_id: {}] Processing remaining {} samples...", trace_id, buffer.size());
         RequestOptions options;
         auto results = engine_->transcribe_pcm16(buffer, 16000, options);
         for (const auto& res : results) {
             if (!res.text.empty()) {
                 sentiric::stt::v1::WhisperTranscribeStreamResponse response;
                 response.set_transcription(res.text);
                 response.set_is_final(true);
                 stream->Write(response);
             }
         }
    }

    return grpc::Status::OK;
}
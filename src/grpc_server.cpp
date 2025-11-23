#include "grpc_server.h"
#include "utils.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <vector>
#include <cstring>

using namespace sentiric::utils;

GrpcServer::GrpcServer(std::shared_ptr<SttEngine> engine, AppMetrics& metrics)
    : engine_(std::move(engine)), metrics_(metrics) {}

grpc::Status GrpcServer::WhisperTranscribe(
    grpc::ServerContext* context,
    const sentiric::stt::v1::WhisperTranscribeRequest* request,
    sentiric::stt::v1::WhisperTranscribeResponse* response)
{
    (void)context;
    metrics_.requests_total.Increment();
    auto start_time = std::chrono::steady_clock::now();
    if (!engine_->is_ready()) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model not ready");
    const std::string& audio_blob = request->audio_data();
    if (audio_blob.empty()) return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Audio data is empty");
    DecodedAudio audio;
    try { audio = parse_wav_robust(audio_blob); } catch (const std::exception& e) {
        spdlog::error("gRPC Audio Parse Error: {}", e.what());
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, std::string("Invalid audio format: ") + e.what());
    }
    if (audio.pcm_data.empty()) return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Audio contains no data");
    RequestOptions options; if (request->has_language()) options.language = request->language();
    auto results = engine_->transcribe_pcm16(audio.pcm_data, audio.sample_rate, options);
    std::string full_text; float avg_prob = 0.0f; std::string main_lang = "unknown";
    if (!results.empty()) {
        main_lang = results[0].language;
        for (const auto& res : results) { full_text += res.text; avg_prob += res.prob; }
        avg_prob /= results.size();
    }
    response->set_transcription(full_text);
    response->set_language(main_lang);
    response->set_language_probability(avg_prob);
    double duration_sec = static_cast<double>(audio.pcm_data.size()) / static_cast<double>(audio.sample_rate);
    response->set_duration(duration_sec);
    // ---- affective ----
    if (!results.empty()) {
        const auto& aff = results[0].affective;
        response->set_gender_proxy(aff.gender_proxy);
        response->set_emotion_proxy(aff.emotion_proxy);
        response->set_arousal(aff.arousal);
        response->set_valence(aff.valence);
        // yeniler: proto'ya ekledik
        response->set_pitch_mean(aff.pitch_mean);
        response->set_pitch_std(aff.pitch_std);
        response->set_energy_mean(aff.energy_mean);
        response->set_energy_std(aff.energy_std);
        response->set_spectral_centroid(aff.spectral_centroid);
        response->set_zero_crossing_rate(aff.zero_crossing_rate);
        // speaker_vec
        for (float v : aff.speaker_vec) response->add_speaker_vec(v);
    }
    metrics_.audio_seconds_processed_total.Increment(duration_sec);
    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> latency = end_time - start_time;
    metrics_.request_latency.Observe(latency.count());
    spdlog::info("gRPC Unary: {:.2f}s Audio (SR:{}), Latency: {:.2f}s", duration_sec, audio.sample_rate, latency.count());
    return grpc::Status::OK;
}

grpc::Status GrpcServer::WhisperTranscribeStream(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<sentiric::stt::v1::WhisperTranscribeStreamResponse, sentiric::stt::v1::WhisperTranscribeStreamRequest>* stream)
{
    metrics_.requests_total.Increment();
    auto start_time = std::chrono::steady_clock::now();
    if (!engine_->is_ready()) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model not ready");
    sentiric::stt::v1::WhisperTranscribeStreamRequest request;
    std::vector<int16_t> full_pcm_buffer;
    bool is_first_chunk = true; bool is_wav_container = false; size_t wav_header_skip = 0;
    while (stream->Read(&request)) {
        const std::string& chunk = request.audio_chunk();
        if (chunk.empty()) continue;
        if (is_first_chunk) {
            if (has_wav_header(chunk)) {
                spdlog::info("Stream started with WAV header. Will attempt to skip header bytes.");
                is_wav_container = true; if (chunk.size() > 44) wav_header_skip = 44;
            }
            is_first_chunk = false;
        }
        const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(chunk.data());
        size_t data_len = chunk.size();
        if (is_wav_container && wav_header_skip > 0) {
            if (data_len >= wav_header_skip) { data_ptr += wav_header_skip; data_len -= wav_header_skip; wav_header_skip = 0; }
            else { wav_header_skip -= data_len; data_len = 0; }
        }
        if (data_len > 0) {
            size_t samples = data_len / 2; size_t current_size = full_pcm_buffer.size();
            full_pcm_buffer.resize(current_size + samples);
            std::memcpy(full_pcm_buffer.data() + current_size, data_ptr, samples * 2);
        }
    }
    if (context->IsCancelled()) { spdlog::warn("Stream cancelled by client"); return grpc::Status::CANCELLED; }
    RequestOptions options;
    auto results = engine_->transcribe_pcm16(full_pcm_buffer, 16000, options);
    for (size_t i = 0; i < results.size(); ++i) {
        sentiric::stt::v1::WhisperTranscribeStreamResponse response;
        response.set_transcription(results[i].text);
        response.set_is_final(i == results.size() - 1);
        const auto& aff = results[i].affective;
        response.set_gender_proxy(aff.gender_proxy);
        response.set_emotion_proxy(aff.emotion_proxy);
        response.set_arousal(aff.arousal);
        response.set_valence(aff.valence);
        response.set_pitch_mean(aff.pitch_mean);
        response.set_pitch_std(aff.pitch_std);
        response.set_energy_mean(aff.energy_mean);
        response.set_energy_std(aff.energy_std);
        response.set_spectral_centroid(aff.spectral_centroid);
        response.set_zero_crossing_rate(aff.zero_crossing_rate);
        for (float v : aff.speaker_vec) response.add_speaker_vec(v);
        stream->Write(response);
    }
    double duration_sec = static_cast<double>(full_pcm_buffer.size()) / 16000.0;
    metrics_.audio_seconds_processed_total.Increment(duration_sec);
    auto end_time = std::chrono::steady_clock::now(); std::chrono::duration<double> latency = end_time - start_time;
    metrics_.request_latency.Observe(latency.count());
    spdlog::info("gRPC Stream Finished. Processed {:.2f}s", duration_sec);
    return grpc::Status::OK;
}
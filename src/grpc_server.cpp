// Dosya: src/grpc_server.cpp
#include "grpc_server.h"

#include <chrono>
#include <cstring>
#include <functional>
#include <vector>

#include "suts_logger.h"
#include "utils.h"

using namespace sentiric::utils;

GrpcServer::GrpcServer(std::shared_ptr<SttEngine> engine, AppMetrics& metrics)
    : engine_(std::move(engine)), metrics_(metrics) {}

grpc::Status GrpcServer::WhisperTranscribe(
    grpc::ServerContext* context,
    const sentiric::stt::v1::WhisperTranscribeRequest* request,
    sentiric::stt::v1::WhisperTranscribeResponse* response) {
  std::string trace_id = "unknown", span_id = "unknown", tenant_id = "unknown";
  auto metadata = context->client_metadata();

  if (auto it = metadata.find("x-trace-id"); it != metadata.end())
    trace_id = std::string(it->second.data(), it->second.length());
  if (auto it = metadata.find("x-span-id"); it != metadata.end())
    span_id = std::string(it->second.data(), it->second.length());
  if (auto it = metadata.find("x-tenant-id"); it != metadata.end())
    tenant_id = std::string(it->second.data(), it->second.length());

  if (tenant_id == "unknown" || tenant_id.empty()) {
    SUTS_ERROR("MISSING_TENANT_ID", trace_id, span_id, tenant_id,
               "Tenant ID is missing in gRPC metadata. Request rejected.");
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "tenant_id is strictly required for isolation");
  }

  metrics_.requests_total.Increment();
  auto start_time = std::chrono::steady_clock::now();
  SUTS_INFO("STT_UNARY_REQUEST", trace_id, span_id, tenant_id,
            "📡 Unary gRPC Transcribe requested.");

  if (!engine_->is_ready())
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model not ready");

  DecodedAudio audio;
  try {
    audio = parse_wav_robust(request->audio_data());
  } catch (...) {
    SUTS_ERROR("STT_INVALID_AUDIO", trace_id, span_id, tenant_id,
               "Invalid audio format received.");
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid audio");
  }

  RequestOptions options;
  if (request->has_language()) options.language = request->language();

  auto results =
      engine_->transcribe_pcm16(audio.pcm_data, audio.sample_rate, options);

  if (!results.empty()) {
    response->set_transcription(results[0].text);
    response->set_language(results[0].language);

    // [ARCH-COMPLIANCE FIX]: Zengin Veri Yüklemesi
    const auto& aff = results[0].affective;
    response->set_gender_proxy(aff.gender_proxy);
    response->set_emotion_proxy(aff.emotion_proxy);
    response->set_arousal(aff.arousal);
    response->set_valence(aff.valence);
    response->set_pitch_mean(aff.pitch_mean);
    response->set_pitch_std(aff.pitch_std);
    response->set_energy_mean(aff.energy_mean);
    response->set_energy_std(aff.energy_std);
    response->set_spectral_centroid(aff.spectral_centroid);
    response->set_zero_crossing_rate(aff.zero_crossing_rate);

    for (float v : aff.speaker_vec) {
      response->add_speaker_vec(v);
    }

    response->set_speaker_id(results[0].speaker_id);

    for (const auto& token : results[0].tokens) {
      auto* word_data = response->add_words();
      word_data->set_word(token.text);
      word_data->set_start(static_cast<float>(token.t0) / 100.0f);
      word_data->set_end(static_cast<float>(token.t1) / 100.0f);
      word_data->set_probability(token.p);
    }
  }

  SUTS_INFO("STT_UNARY_COMPLETE", trace_id, span_id, tenant_id,
            "✅ Unary transcription completed.");
  return grpc::Status::OK;
}

grpc::Status GrpcServer::WhisperTranscribeStream(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<sentiric::stt::v1::WhisperTranscribeStreamResponse,
                             sentiric::stt::v1::WhisperTranscribeStreamRequest>*
        stream) {
  std::string trace_id = "unknown", span_id = "unknown", tenant_id = "unknown";
  auto metadata = context->client_metadata();

  if (auto it = metadata.find("x-trace-id"); it != metadata.end())
    trace_id = std::string(it->second.data(), it->second.length());
  if (auto it = metadata.find("x-span-id"); it != metadata.end())
    span_id = std::string(it->second.data(), it->second.length());
  if (auto it = metadata.find("x-tenant-id"); it != metadata.end())
    tenant_id = std::string(it->second.data(), it->second.length());

  if (tenant_id == "unknown" || tenant_id.empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "tenant_id is strictly required for isolation");
  }

  metrics_.requests_total.Increment();
  SUTS_INFO("STT_STREAM_STARTED", trace_id, span_id, tenant_id,
            "📡 New gRPC Stream Connection started.");

  if (!engine_->is_ready())
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model not ready");

  sentiric::stt::v1::WhisperTranscribeStreamRequest request;
  std::vector<int16_t> buffer;

  // [YENİ MİMARİ]: Kesintisiz tampon yönetimi
  size_t last_processed_size = 0;
  const size_t DYNAMIC_BUFFER_SIZE =
      engine_->get_settings().stream_buffer_samples;
  const size_t MAX_BUFFER_SIZE = 16000 * 30;  // 30 Saniye maksimum sınır

  bool is_first_chunk = true;
  bool is_wav_container = false;
  size_t wav_header_skip = 0;

  while (stream->Read(&request)) {
    if (context->IsCancelled()) return grpc::Status::CANCELLED;

    const std::string& chunk = request.audio_chunk();

    // [YENİ]: EOS SİNYALİ (İstemci Sustuğunda Tetiklenir)
    if (chunk.empty()) {
      if (!buffer.empty()) {
        SUTS_DEBUG("STT_EOS_RECEIVED", trace_id, span_id, tenant_id,
                   "EOS signal received. Finalizing {} samples.",
                   buffer.size());
        RequestOptions options;
        auto results = engine_->transcribe_pcm16(buffer, 16000, options);
        for (const auto& res : results) {
          if (!res.text.empty()) {
            sentiric::stt::v1::WhisperTranscribeStreamResponse response;
            response.set_transcription(res.text);
            // [KRİTİK]: Cümle Bitti!
            response.set_is_final(true);

            const auto& aff = res.affective;
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
            response.set_speaker_id(res.speaker_id);

            for (const auto& token : res.tokens) {
              auto* word_data = response.add_words();
              word_data->set_word(token.text);
              word_data->set_start(static_cast<float>(token.t0) / 100.0f);
              word_data->set_end(static_cast<float>(token.t1) / 100.0f);
              word_data->set_probability(token.p);
            }
            stream->Write(response);
            SUTS_INFO("STT_TRANSCRIPT_FINALIZED", trace_id, span_id, tenant_id,
                      "✅ Final Sentence: '{}' [Spk: {}]", res.text,
                      res.speaker_id);
          }
        }
        // Cümle bitince yeni cümle için tamponu sıfırla
        buffer.clear();
        last_processed_size = 0;
      }
      continue;
    }

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
        data_ptr += wav_header_skip;
        data_len -= wav_header_skip;
        wav_header_skip = 0;
      } else {
        wav_header_skip -= data_len;
        data_len = 0;
      }
    }

    if (data_len > 0) {
      size_t samples = data_len / 2;
      size_t current_size = buffer.size();
      buffer.resize(current_size + samples);
      std::memcpy(buffer.data() + current_size, data_ptr, samples * 2);
    }

    // [YENİ]: TAMPONU TEMİZLEMEDEN (Partial) İŞLEME
    if (buffer.size() - last_processed_size >= DYNAMIC_BUFFER_SIZE) {
      RequestOptions options;
      SttEngine::PerformanceMetrics perf;

      try {
        auto results = engine_->transcribe_pcm16(buffer, 16000, options, &perf);
        last_processed_size = buffer.size();

        // [MİMARİ DÜZELTME]: Partial mesajlarda (Kullanıcı hala konuşurken)
        // Whisper birden fazla segment bulursa, UI bunları tek tek alıp ezmesin
        // diye Hepsini tek bir string olarak birleştirip gönderiyoruz.
        std::string combined_partial_text;
        sentiric::stt::v1::WhisperTranscribeStreamResponse combined_response;
        bool has_valid_data = false;

        for (const auto& res : results) {
          if (!res.text.empty()) {
            combined_partial_text += res.text + " ";
            has_valid_data = true;

            // Son segmentin duygu durumunu ve vektörünü al (En güncel olan)
            const auto& aff = res.affective;
            combined_response.set_gender_proxy(aff.gender_proxy);
            combined_response.set_emotion_proxy(aff.emotion_proxy);
            combined_response.set_arousal(aff.arousal);
            combined_response.set_valence(aff.valence);
            combined_response.set_pitch_mean(aff.pitch_mean);
            combined_response.set_pitch_std(aff.pitch_std);
            combined_response.set_energy_mean(aff.energy_mean);
            combined_response.set_energy_std(aff.energy_std);
            combined_response.set_spectral_centroid(aff.spectral_centroid);
            combined_response.set_zero_crossing_rate(aff.zero_crossing_rate);

            combined_response.clear_speaker_vec();
            for (float v : aff.speaker_vec)
              combined_response.add_speaker_vec(v);

            combined_response.set_speaker_id(res.speaker_id);
          }
        }

        if (has_valid_data) {
          combined_response.set_transcription(combined_partial_text);
          combined_response.set_is_final(false);  // Hala konuşuyor
          if (!stream->Write(combined_response)) {
            return grpc::Status::OK;
          }
        }

        // [KRİTİK VERİ KAYBI ÇÖZÜMÜ]: OOM Koruması (30 Saniye Sınırı)
        // Kullanıcı 30sn susmadan konuşursa, buffer'ı silmeden önce her şeyi
        // FINAL olarak kaydet!
        if (buffer.size() > MAX_BUFFER_SIZE) {
          SUTS_WARN("STT_BUFFER_OVERFLOW", trace_id, span_id, tenant_id,
                    "User spoke for 30s without breathing. Forcing "
                    "finalization to prevent data loss.");

          for (const auto& res : results) {
            if (!res.text.empty()) {
              sentiric::stt::v1::WhisperTranscribeStreamResponse final_resp;
              final_resp.set_transcription(res.text);
              final_resp.set_is_final(true);  // ZORLA FİNAL YAP

              const auto& aff = res.affective;
              final_resp.set_gender_proxy(aff.gender_proxy);
              final_resp.set_emotion_proxy(aff.emotion_proxy);
              final_resp.set_arousal(aff.arousal);
              final_resp.set_valence(aff.valence);
              final_resp.set_speaker_id(res.speaker_id);
              for (float v : aff.speaker_vec) final_resp.add_speaker_vec(v);

              stream->Write(final_resp);
            }
          }
          buffer.clear();
          last_processed_size = 0;
        }

      } catch (const std::exception& e) {
        SUTS_ERROR("STT_STREAM_ERROR", trace_id, span_id, tenant_id,
                   "Streaming error: {}", e.what());
      }
    }
  }

  SUTS_INFO("STT_STREAM_COMPLETED", trace_id, span_id, tenant_id,
            "✅ gRPC Stream Connection closed cleanly.");
  return grpc::Status::OK;
}
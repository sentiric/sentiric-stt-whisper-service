#pragma once

#include "stt_engine.h"
#include "http_server.h" // AppMetrics için
#include <memory>
#include <grpcpp/grpcpp.h>
#include "sentiric/stt/v1/whisper.grpc.pb.h"

class GrpcServer final : public sentiric::stt::v1::SttWhisperService::Service {
public:
    explicit GrpcServer(std::shared_ptr<SttEngine> engine, AppMetrics& metrics);

    // Tekil Dosya Transkripsiyonu
    grpc::Status WhisperTranscribe(
        grpc::ServerContext* context,
        const sentiric::stt::v1::WhisperTranscribeRequest* request,
        sentiric::stt::v1::WhisperTranscribeResponse* response
    ) override;

    // Akış (Streaming) Transkripsiyonu
    grpc::Status WhisperTranscribeStream(
        grpc::ServerContext* context,
        grpc::ServerReaderWriter<sentiric::stt::v1::WhisperTranscribeStreamResponse, sentiric::stt::v1::WhisperTranscribeStreamRequest>* stream
    ) override;

private:
    std::shared_ptr<SttEngine> engine_;
    AppMetrics& metrics_;
};
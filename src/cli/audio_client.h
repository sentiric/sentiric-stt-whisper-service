#pragma once

#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "sentiric/stt/v1/whisper.grpc.pb.h"

class AudioClient {
public:
    AudioClient(const std::string& address);
    
    // Tekil dosya gönderimi
    void transcribe_file(const std::string& filepath);
    
    // Akış (Streaming) testi
    void transcribe_stream(const std::string& filepath);

private:
    std::unique_ptr<sentiric::stt::v1::SttWhisperService::Stub> stub_;
};
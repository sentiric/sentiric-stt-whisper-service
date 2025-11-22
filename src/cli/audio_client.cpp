#include "audio_client.h"
#include "spdlog/spdlog.h"
#include "fmt/format.h" // fmt::formatter için gerekli
#include <fstream>
#include <vector>
#include <thread>
#include <string_view>

// --- FMT FORMATTER SPECIALIZATION (DÜZELTME) ---
// grpc::StatusCode enum'ını string'e çevirmek için öğretici.
template <>
struct fmt::formatter<grpc::StatusCode> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const grpc::StatusCode& code, FormatContext& ctx) const -> decltype(ctx.out()) {
        std::string_view name = "UNKNOWN";
        switch (code) {
            case grpc::StatusCode::OK: name = "OK"; break;
            case grpc::StatusCode::CANCELLED: name = "CANCELLED"; break;
            case grpc::StatusCode::UNKNOWN: name = "UNKNOWN"; break;
            case grpc::StatusCode::INVALID_ARGUMENT: name = "INVALID_ARGUMENT"; break;
            case grpc::StatusCode::DEADLINE_EXCEEDED: name = "DEADLINE_EXCEEDED"; break;
            case grpc::StatusCode::NOT_FOUND: name = "NOT_FOUND"; break;
            case grpc::StatusCode::ALREADY_EXISTS: name = "ALREADY_EXISTS"; break;
            case grpc::StatusCode::PERMISSION_DENIED: name = "PERMISSION_DENIED"; break;
            case grpc::StatusCode::UNAUTHENTICATED: name = "UNAUTHENTICATED"; break;
            case grpc::StatusCode::RESOURCE_EXHAUSTED: name = "RESOURCE_EXHAUSTED"; break;
            case grpc::StatusCode::FAILED_PRECONDITION: name = "FAILED_PRECONDITION"; break;
            case grpc::StatusCode::ABORTED: name = "ABORTED"; break;
            case grpc::StatusCode::OUT_OF_RANGE: name = "OUT_OF_RANGE"; break;
            case grpc::StatusCode::UNIMPLEMENTED: name = "UNIMPLEMENTED"; break;
            case grpc::StatusCode::INTERNAL: name = "INTERNAL"; break;
            case grpc::StatusCode::UNAVAILABLE: name = "UNAVAILABLE"; break;
            case grpc::StatusCode::DATA_LOSS: name = "DATA_LOSS"; break;
            default: break;
        }
        return fmt::format_to(ctx.out(), "{} ({})", name, static_cast<int>(code));
    }
};
// -----------------------------------------------

AudioClient::AudioClient(const std::string& address) {
    // CLI için şimdilik Insecure channel yeterli, prod CLI için SSL eklenebilir
    auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    stub_ = sentiric::stt::v1::SttWhisperService::NewStub(channel);
}

// Basit WAV okuyucu (Header'ı atla)
std::vector<char> read_wav_body(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("File not found");
    
    // Dosya boyutunu al
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    // WAV header'ını (44 byte) atla (Basit yaklaşım)
    // Uyarı: Gerçek bir WAV parser kullanmak daha güvenlidir.
    if (size > 44) {
        file.seekg(44);
        size -= 44;
    }

    std::vector<char> buffer(size);
    file.read(buffer.data(), size);
    return buffer;
}

void AudioClient::transcribe_file(const std::string& filepath) {
    grpc::ClientContext context;
    sentiric::stt::v1::WhisperTranscribeRequest request;
    sentiric::stt::v1::WhisperTranscribeResponse response;

    try {
        auto audio_data = read_wav_body(filepath);
        request.set_audio_data(audio_data.data(), audio_data.size());
        request.set_language("auto");

        spdlog::info("Sending file: {} ({} bytes)", filepath, audio_data.size());
        
        grpc::Status status = stub_->WhisperTranscribe(&context, request, &response);

        if (status.ok()) {
            spdlog::info("✅ Transcription Success!");
            spdlog::info("Text: {}", response.transcription());
            spdlog::info("Lang: {} (Prob: {:.2f})", response.language(), response.language_probability());
            spdlog::info("Duration: {:.2f}s", response.duration());
        } else {
            spdlog::error("RPC Failed: {}", status.error_code()); // ARTIK ÇALIŞACAK
        }
    } catch (const std::exception& e) {
        spdlog::error("Client Error: {}", e.what());
    }
}

void AudioClient::transcribe_stream(const std::string& filepath) {
    grpc::ClientContext context;
    auto stream = stub_->WhisperTranscribeStream(&context);

    try {
        auto audio_data = read_wav_body(filepath);
        size_t chunk_size = 16000 * 2 * 0.5; // 0.5 saniyelik chunklar (16kHz * 16bit)
        size_t offset = 0;

        spdlog::info("Streaming file: {}", filepath);

        while (offset < audio_data.size()) {
            size_t current_chunk_size = std::min(chunk_size, audio_data.size() - offset);
            
            sentiric::stt::v1::WhisperTranscribeStreamRequest req;
            req.set_audio_chunk(audio_data.data() + offset, current_chunk_size);
            
            if (!stream->Write(req)) {
                spdlog::error("Stream write failed.");
                break;
            }

            offset += current_chunk_size;
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Gerçek zamanlı simülasyon
            std::cout << "." << std::flush;
        }
        
        stream->WritesDone();
        
        sentiric::stt::v1::WhisperTranscribeStreamResponse resp;
        while (stream->Read(&resp)) {
            spdlog::info("Stream Recv: {} (Final: {})", resp.transcription(), resp.is_final());
        }

        grpc::Status status = stream->Finish();
        if (!status.ok()) {
            spdlog::error("Stream RPC Failed: {}", status.error_message());
        } else {
            spdlog::info("✅ Streaming Completed.");
        }

    } catch (const std::exception& e) {
        spdlog::error("Stream Client Error: {}", e.what());
    }
}
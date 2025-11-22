#include "audio_client.h"
#include "spdlog/spdlog.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: stt_cli <file|stream> <wav_file_path> [grpc_address]" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    std::string filepath = argv[2];
    std::string address = (argc > 3) ? argv[3] : "localhost:15031";

    AudioClient client(address);

    if (mode == "file") {
        client.transcribe_file(filepath);
    } else if (mode == "stream") {
        client.transcribe_stream(filepath);
    } else {
        spdlog::error("Unknown mode: {}", mode);
        return 1;
    }

    return 0;
}
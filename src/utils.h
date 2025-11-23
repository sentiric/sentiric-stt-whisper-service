#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include "spdlog/spdlog.h"

namespace sentiric::utils {

    // İşlenmiş ses verisi yapısı
    struct DecodedAudio {
        std::vector<int16_t> pcm_data;
        int sample_rate = 16000;
        int channels = 1;
    };

    // UTF-8 Temizleme (Invalid byte sequence'ları atlar)
    inline std::string clean_utf8(const std::string& str) {
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

    // Binary veri dökümü (Debug için)
    inline std::string hex_dump(const void* data, size_t size) {
        const unsigned char* p = static_cast<const unsigned char*>(data);
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < size; ++i) {
            ss << std::setw(2) << static_cast<int>(p[i]) << " ";
        }
        return ss.str();
    }

    // Sağlam WAV Parser ve Mono Converter
    inline DecodedAudio parse_wav_robust(const std::string& bytes) {
        DecodedAudio result;
        if (bytes.size() < 44) throw std::runtime_error("WAV too small (<44 bytes)");

        const uint8_t* data = reinterpret_cast<const uint8_t*>(bytes.data());
        size_t ptr = 0;

        if (std::memcmp(data + ptr, "RIFF", 4) != 0) throw std::runtime_error("Invalid WAV: No RIFF");
        ptr += 8; 
        if (std::memcmp(data + ptr, "WAVE", 4) != 0) throw std::runtime_error("Invalid WAV: No WAVE");
        ptr += 4;

        const uint8_t* pcm_start = nullptr;
        size_t pcm_size_bytes = 0;
        int16_t bits_per_sample = 0;
        bool fmt_found = false;

        while (ptr + 8 < bytes.size()) {
            char chunk_id[5] = {0};
            std::memcpy(chunk_id, data + ptr, 4);
            ptr += 4;

            uint32_t chunk_size;
            std::memcpy(&chunk_size, data + ptr, 4);
            ptr += 4;

            if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
                // --- SELF-HEALING LOGIC ---
                if (chunk_size == 0) {
                    spdlog::warn("⚠️ Malformed WAV detected: 'fmt ' chunk size is 0. Attempting repair (Assuming standard PCM 16).");
                    chunk_size = 16; 
                }
                // --------------------------

                if (chunk_size < 16) {
                    throw std::runtime_error("Invalid fmt chunk size: " + std::to_string(chunk_size));
                }
                
                uint16_t format_tag;
                std::memcpy(&format_tag, data + ptr, 2);
                if (format_tag != 1 && format_tag != 0xFFFE) {
                    throw std::runtime_error("Unsupported format_tag: " + std::to_string(format_tag));
                }

                std::memcpy(&result.channels, data + ptr + 2, 2);
                std::memcpy(&result.sample_rate, data + ptr + 4, 4);
                std::memcpy(&bits_per_sample, data + ptr + 14, 2);
                
                fmt_found = true;
                ptr += chunk_size;
            } 
            else if (std::memcmp(chunk_id, "data", 4) == 0) {
                if (!fmt_found) throw std::runtime_error("Found data chunk before fmt chunk");
                
                pcm_start = data + ptr;
                pcm_size_bytes = chunk_size;
                break; 
            } 
            else {
                ptr += chunk_size;
            }

            if (chunk_size % 2 != 0) ptr++;
        }

        if (!pcm_start || pcm_size_bytes == 0) throw std::runtime_error("No 'data' chunk found");

        if (bits_per_sample != 16) throw std::runtime_error("Unsupported bit depth: " + std::to_string(bits_per_sample));

        size_t remaining = bytes.size() - (pcm_start - data);
        if (pcm_size_bytes > remaining) {
            spdlog::warn("WAV data chunk size ({}) > remaining bytes ({})! Truncating.", pcm_size_bytes, remaining);
            pcm_size_bytes = remaining;
        }

        size_t num_samples = pcm_size_bytes / 2;
        const int16_t* raw_samples = reinterpret_cast<const int16_t*>(pcm_start);

        if (result.channels == 1) {
            result.pcm_data.assign(raw_samples, raw_samples + num_samples);
        } 
        else if (result.channels == 2) {
            size_t frames = num_samples / 2;
            result.pcm_data.resize(frames);
            for (size_t i = 0; i < frames; ++i) {
                int32_t mixed = (int32_t)raw_samples[i*2] + (int32_t)raw_samples[i*2 + 1];
                result.pcm_data[i] = static_cast<int16_t>(mixed / 2);
            }
        } 
        else {
            // Multichannel -> Mono (Sadece ilk kanalı al, mix yapmak karmaşık olabilir)
            size_t frames = num_samples / result.channels;
            result.pcm_data.resize(frames);
            for (size_t i = 0; i < frames; ++i) {
                result.pcm_data[i] = raw_samples[i * result.channels];
            }
        }

        return result;
    }
}
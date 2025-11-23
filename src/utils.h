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
        bool is_valid = false;
    };

    // UTF-8 Temizleme
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

    // Binary veri dökümü
    inline std::string hex_dump(const void* data, size_t size) {
        const unsigned char* p = static_cast<const unsigned char*>(data);
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < size; ++i) {
            ss << std::setw(2) << static_cast<int>(p[i]) << " ";
        }
        return ss.str();
    }

    // Basit WAV Header Kontrolü (RIFF + WAVE)
    inline bool has_wav_header(const std::string& bytes) {
        if (bytes.size() < 12) return false;
        return (std::memcmp(bytes.data(), "RIFF", 4) == 0) && 
               (std::memcmp(bytes.data() + 8, "WAVE", 4) == 0);
    }

    // Sağlam WAV Parser
    inline DecodedAudio parse_wav_robust(const std::string& bytes) {
        DecodedAudio result;
        result.is_valid = false;

        // Eğer WAV header yoksa, Raw PCM varsay
        if (!has_wav_header(bytes)) {
            // PCM 16-bit varsayımı (Header yoksa)
            if (bytes.size() % 2 != 0) {
                 // Tek sayı ise son byte'ı at, alignment koru
                 spdlog::warn("Raw PCM data size is odd ({}), truncating last byte.", bytes.size());
            }
            size_t samples = bytes.size() / 2;
            result.pcm_data.resize(samples);
            if (samples > 0) {
                std::memcpy(result.pcm_data.data(), bytes.data(), samples * 2);
            }
            result.sample_rate = 16000; // Varsayılan
            result.channels = 1;
            result.is_valid = true;
            return result;
        }

        // WAV Parsing
        const uint8_t* data = reinterpret_cast<const uint8_t*>(bytes.data());
        size_t ptr = 12; // RIFF + Size + WAVE atlandı

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

            // Bounds check for chunk body
            if (ptr + chunk_size > bytes.size()) {
                spdlog::warn("WAV chunk '{}' size {} exceeds file boundaries.", chunk_id, chunk_size);
                break;
            }

            if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
                if (chunk_size < 16) throw std::runtime_error("Invalid fmt chunk size");
                
                uint16_t format_tag;
                std::memcpy(&format_tag, data + ptr, 2);
                // 1=PCM, 0xFFFE=Extensible
                if (format_tag != 1 && format_tag != 0xFFFE) {
                    throw std::runtime_error("Unsupported WAV format tag: " + std::to_string(format_tag));
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
                
                // DATA chunk bulundu, ancak dosya sonunda başka chunklar (LIST vb.) olabilir.
                // Şimdilik burada break yapıyoruz çünkü stream edilebilir WAV'lar için data son chunk olmayabilir
                // ama pointer'ı kaydedip çıkmak en güvenlisi.
                break; 
            } 
            else {
                ptr += chunk_size;
            }

            // Padding byte skip (Word alignment)
            if (chunk_size % 2 != 0) {
                if (ptr < bytes.size()) ptr++;
            }
        }

        if (!pcm_start || pcm_size_bytes == 0) throw std::runtime_error("No 'data' chunk found in WAV");
        if (bits_per_sample != 16) throw std::runtime_error("Unsupported bit depth: " + std::to_string(bits_per_sample));

        // Truncate check
        size_t remaining = bytes.size() - (pcm_start - data);
        if (pcm_size_bytes > remaining) pcm_size_bytes = remaining;

        size_t num_samples = pcm_size_bytes / 2;
        const int16_t* raw_samples = reinterpret_cast<const int16_t*>(pcm_start);

        // Channel Mixing / Copy
        if (result.channels == 1) {
            result.pcm_data.assign(raw_samples, raw_samples + num_samples);
        } 
        else if (result.channels == 2) {
            // Stereo -> Mono (Average)
            size_t frames = num_samples / 2;
            result.pcm_data.resize(frames);
            for (size_t i = 0; i < frames; ++i) {
                int32_t mixed = (int32_t)raw_samples[i*2] + (int32_t)raw_samples[i*2 + 1];
                result.pcm_data[i] = static_cast<int16_t>(mixed / 2);
            }
        } 
        else {
            // Multi -> Mono (Take Channel 0)
            size_t frames = num_samples / result.channels;
            result.pcm_data.resize(frames);
            for (size_t i = 0; i < frames; ++i) {
                result.pcm_data[i] = raw_samples[i * result.channels];
            }
        }

        result.is_valid = true;
        return result;
    }
}
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <array>
#include <fstream>
#include <cstdlib>
#include <random>
#include "spdlog/spdlog.h"

namespace sentiric::utils {

    struct DecodedAudio {
        std::vector<int16_t> pcm_data;
        int sample_rate = 16000;
        int channels = 1;
        bool is_valid = false;
    };

    // --- YENİ EKLENEN FONKSİYON: FFmpeg ile MP3/WebM -> WAV PCM Dönüştürme ---
    // Bu versiyon popen yerine daha güvenli olan dosya tabanlı dönüşümü kullanır.
    inline std::vector<int16_t> decode_with_ffmpeg(const std::string& input_data) {
        std::vector<int16_t> output;
        
        // Rastgele dosya ismi oluştur (Thread-safety için basit random)
        int rand_id = std::rand();
        std::string temp_in = "/tmp/stt_in_" + std::to_string(rand_id) + ".bin";
        std::string temp_out = "/tmp/stt_out_" + std::to_string(rand_id) + ".raw";
        
        // 1. Gelen veriyi (MP3/WebM) diske yaz
        std::ofstream outfile(temp_in, std::ios::binary);
        if (!outfile.is_open()) {
            spdlog::error("Temp file write failed: {}", temp_in);
            return output;
        }
        outfile.write(input_data.data(), input_data.size());
        outfile.close();
        
        // 2. FFmpeg ile dönüştür (Sessiz modda)
        // -y: Üzerine yaz
        // -f s16le -acodec pcm_s16le: Ham 16-bit PCM çıktısı
        // -ac 1: Mono
        // -ar 16000: 16kHz
        std::string cmd = "ffmpeg -y -hide_banner -loglevel error -i " + temp_in + " -f s16le -acodec pcm_s16le -ac 1 -ar 16000 " + temp_out;
        
        int ret = std::system(cmd.c_str());
        
        if (ret == 0) {
            // 3. Dönüştürülen Raw PCM verisini oku
            std::ifstream infile(temp_out, std::ios::binary | std::ios::ate);
            if (infile.is_open()) {
                std::streamsize size = infile.tellg();
                infile.seekg(0, std::ios::beg);
                
                // Boyut kontrolü (Çok küçükse hata olabilir)
                if (size > 0) {
                    output.resize(size / 2); // 16-bit = 2 byte
                    infile.read(reinterpret_cast<char*>(output.data()), size);
                    spdlog::info("FFmpeg conversion success: {} bytes -> {} samples", size, output.size());
                }
            }
        } else {
            spdlog::error("FFmpeg conversion failed with return code: {}", ret);
        }
        
        // 4. Temizlik (Her durumda sil)
        std::remove(temp_in.c_str());
        std::remove(temp_out.c_str());
        
        return output;
    }
    // -----------------------------------------------------------------------

    inline std::string clean_utf8(const std::string& str) {
        std::string res; res.reserve(str.size()); size_t i = 0;
        while (i < str.size()) { unsigned char c = str[i]; int n; if(c<0x80)n=1; else if((c&0xE0)==0xC0)n=2; else if((c&0xF0)==0xE0)n=3; else if((c&0xF8)==0xF0)n=4; else{i++;continue;} if(i+n>str.size())break; bool valid=true; for(int j=1;j<n;j++)if((str[i+j]&0xC0)!=0x80){valid=false;break;} if(valid){res.append(str,i,n);i+=n;}else{i++;} } return res;
    }

    inline bool has_wav_header(const std::string& bytes) {
        if (bytes.size() < 12) return false;
        return (std::memcmp(bytes.data(), "RIFF", 4) == 0) && 
               (std::memcmp(bytes.data() + 8, "WAVE", 4) == 0);
    }

    inline DecodedAudio parse_wav_robust(const std::string& bytes) {
        DecodedAudio result;
        result.is_valid = false;

        // WAV Header yoksa (Muhtemelen MP3, WebM veya Raw PCM)
        if (!has_wav_header(bytes)) {
            
            spdlog::info("No WAV header found. Attempting FFmpeg conversion...");
            std::vector<int16_t> converted = decode_with_ffmpeg(bytes);
            
            if (!converted.empty()) {
                result.pcm_data = std::move(converted);
                result.sample_rate = 16000;
                result.channels = 1;
                result.is_valid = true;
                return result;
            }

            // FFmpeg başarısız olursa eski usül Raw PCM varsay (Fallback)
            spdlog::warn("FFmpeg conversion returned empty. Falling back to Raw PCM assumption.");
            if (bytes.size() % 2 != 0) {
                 spdlog::warn("Raw PCM data size is odd ({}), truncating last byte.", bytes.size());
            }
            size_t samples = bytes.size() / 2;
            result.pcm_data.resize(samples);
            if (samples > 0) {
                std::memcpy(result.pcm_data.data(), bytes.data(), samples * 2);
            }
            result.sample_rate = 16000;
            result.channels = 1;
            result.is_valid = true;
            return result;
        }

        // WAV Parsing (Değişmedi)
        const uint8_t* data = reinterpret_cast<const uint8_t*>(bytes.data());
        size_t ptr = 12;
        const uint8_t* pcm_start = nullptr;
        size_t pcm_size_bytes = 0;
        int16_t bits_per_sample = 0;
        bool fmt_found = false;
        while (ptr + 8 < bytes.size()) {
            char chunk_id[5] = {0}; std::memcpy(chunk_id, data + ptr, 4); ptr += 4;
            uint32_t chunk_size; std::memcpy(&chunk_size, data + ptr, 4); ptr += 4;
            if (ptr + chunk_size > bytes.size()) break;
            if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
                if (chunk_size < 16) throw std::runtime_error("Invalid fmt");
                uint16_t format_tag; std::memcpy(&format_tag, data + ptr, 2);
                if (format_tag != 1 && format_tag != 0xFFFE) throw std::runtime_error("Unsupported WAV tag");
                std::memcpy(&result.channels, data + ptr + 2, 2);
                std::memcpy(&result.sample_rate, data + ptr + 4, 4);
                std::memcpy(&bits_per_sample, data + ptr + 14, 2);
                fmt_found = true; ptr += chunk_size;
            } else if (std::memcmp(chunk_id, "data", 4) == 0) {
                if (!fmt_found) throw std::runtime_error("No fmt chunk");
                pcm_start = data + ptr; pcm_size_bytes = chunk_size; break; 
            } else { ptr += chunk_size; }
            if (chunk_size % 2 != 0) { if (ptr < bytes.size()) ptr++; }
        }
        if (!pcm_start || pcm_size_bytes == 0) throw std::runtime_error("No data chunk");
        if (bits_per_sample != 16) throw std::runtime_error("Unsupported bit depth");
        size_t remaining = bytes.size() - (pcm_start - data);
        if (pcm_size_bytes > remaining) pcm_size_bytes = remaining;
        size_t num_samples = pcm_size_bytes / 2;
        const int16_t* raw_samples = reinterpret_cast<const int16_t*>(pcm_start);
        if (result.channels == 1) { result.pcm_data.assign(raw_samples, raw_samples + num_samples); } 
        else if (result.channels == 2) { size_t frames = num_samples / 2; result.pcm_data.resize(frames); for (size_t i = 0; i < frames; ++i) { int32_t mixed = (int32_t)raw_samples[i*2] + (int32_t)raw_samples[i*2 + 1]; result.pcm_data[i] = static_cast<int16_t>(mixed / 2); } } 
        else { size_t frames = num_samples / result.channels; result.pcm_data.resize(frames); for (size_t i = 0; i < frames; ++i) { result.pcm_data[i] = raw_samples[i * result.channels]; } }
        result.is_valid = true;
        return result;
    }
}
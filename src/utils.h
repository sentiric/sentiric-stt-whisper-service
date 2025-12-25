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

    inline std::vector<int16_t> decode_with_ffmpeg(const std::string& input_data) {
        std::vector<int16_t> output;
        int rand_id = std::rand();
        std::string temp_in = "/tmp/stt_in_" + std::to_string(rand_id) + ".bin";
        std::string temp_out = "/tmp/stt_out_" + std::to_string(rand_id) + ".raw";
        
        std::ofstream outfile(temp_in, std::ios::binary);
        if (!outfile.is_open()) {
            spdlog::error("Temp file write failed: {}", temp_in);
            return output;
        }
        outfile.write(input_data.data(), input_data.size());
        outfile.close();
        
        std::string cmd = "ffmpeg -y -hide_banner -loglevel error -i " + temp_in + " -f s16le -acodec pcm_s16le -ac 1 -ar 16000 " + temp_out;
        int ret = std::system(cmd.c_str());
        
        if (ret == 0) {
            std::ifstream infile(temp_out, std::ios::binary | std::ios::ate);
            if (infile.is_open()) {
                std::streamsize size = infile.tellg();
                infile.seekg(0, std::ios::beg);
                if (size > 0) {
                    output.resize(size / 2); 
                    infile.read(reinterpret_cast<char*>(output.data()), size);
                    spdlog::info("FFmpeg conversion success: {} bytes -> {} samples", size, output.size());
                }
            }
        } else {
            spdlog::error("FFmpeg conversion failed with return code: {}", ret);
        }
        std::remove(temp_in.c_str());
        std::remove(temp_out.c_str());
        return output;
    }

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

    inline bool is_hallucination(const std::string& text) {
        if (text.empty()) return true;
        
        // 1. Çok kısa ve anlamsız metinler
        if (text.length() < 2) return true;
        
        // 2. Sadece noktalama işaretleri
        if (text.find_first_not_of(" \t\n\v\f\r.,?!") == std::string::npos) return true;

        // 3. Köşeli parantezli ses efektleri
        if (text.front() == '[' && text.back() == ']') return true;
        if (text.front() == '(' && text.back() == ')') return true;

        // 4. Yasaklı Kelime Listesi (Whisper Hallucinations & Noise Artifacts)
        static const std::vector<std::string> banned = {
            "altyazı", "sesli betimleme", "senkron", "www.", ".com",
            "izlediğiniz için", "teşekkürler", "thank you", "thanks for watching",
            "abone ol", "videoyu beğen", "bir sonraki videoda",
            "devam edecek", "transcription:", "subtitle:",
            "2分", "ご視聴", 
            "I'm going to go", "Okay.", "Bye.",
            // [EKLENEN] Gürültüden kaynaklı sık uydurmalar
            "Hıhı", "Pffft", "Ehem", "Hmm", "Aa", "Ah", "Oh", "Eh" 
        };

        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // Tam eşleşme kontrolü (Kısa kelimeler için)
        // Örneğin "Ah" kelimesi normal bir cümlenin içinde geçebilir ("Ahmet geldi"),
        // ama tek başına "Ah." ise halüsinasyondur.
        
        // Önce "contains" kontrolü (Uzunlar için)
        for (const auto& phrase : banned) {
            if (phrase.length() > 4 && lower.find(phrase) != std::string::npos) return true;
        }
        
        // Sonra "exact match" veya "stripped match" kontrolü (Kısalar için)
        std::string stripped = lower;
        // Sondaki noktalamayı sil
        while (!stripped.empty() && ispunct(stripped.back())) stripped.pop_back();
        while (!stripped.empty() && ispunct(stripped.front())) stripped.erase(0, 1);
        
        for (const auto& phrase : banned) {
            std::string phrase_lower = phrase;
            std::transform(phrase_lower.begin(), phrase_lower.end(), phrase_lower.begin(), ::tolower);
            if (phrase.length() <= 4 && stripped == phrase_lower) return true;
        }

        return false;
    }    
}
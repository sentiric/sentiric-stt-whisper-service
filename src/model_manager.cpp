#include "model_manager.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <cstdlib>
#include <fstream>

namespace fs = std::filesystem;

std::string ModelManager::ensure_model(const Settings& settings) {
    // Model URL şablonunu işle
    std::string model_name = settings.model_filename;
    if (model_name.rfind("ggml-", 0) == 0) model_name.replace(0, 5, "");
    if (model_name.length() >= 4 && model_name.substr(model_name.length() - 4) == ".bin") 
        model_name = model_name.substr(0, model_name.length() - 4);

    std::string url = settings.model_url_template;
    std::string placeholder = "{model_name}";
    size_t pos = url.find(placeholder);
    if (pos != std::string::npos) {
        url.replace(pos, placeholder.length(), model_name);
    }

    // Ana model genelde büyük olur (>50MB), 1MB alt limit
    return ensure_file(settings.model_dir, settings.model_filename, url, 1024 * 1024);
}

std::string ModelManager::ensure_vad_model(const Settings& settings) {
    if (!settings.enable_vad) return "";
    
    // VAD Modeli genelde küçüktür (~20MB), 100KB alt limit (LFS pointer'ı engellemek için)
    return ensure_file(settings.model_dir, settings.vad_model_filename, settings.vad_model_url, 100 * 1024);
}

std::string ModelManager::ensure_file(const std::string& dir, const std::string& filename, const std::string& url, size_t min_size_bytes) {
    fs::path model_dir(dir);
    fs::path model_path = model_dir / filename;

    // 1. Dizin kontrolü
    if (!fs::exists(model_dir)) {
        spdlog::info("Creating directory: {}", dir);
        fs::create_directories(model_dir);
    }

    // 2. Dosya kontrolü
    if (fs::exists(model_path)) {
        size_t size = fs::file_size(model_path);
        if (size > min_size_bytes) {
            spdlog::info("✅ File ready: {} ({:.2f} MB)", filename, (double)size / (1024.0 * 1024.0));
            return model_path.string();
        } else {
            spdlog::warn("⚠️ Corrupted/Small file detected ({} bytes). Deleting and re-downloading...", size);
            fs::remove(model_path);
        }
    }

    // 3. İndirme
    spdlog::info("⬇️ Downloading: {} from {}", filename, url);
    
    if (download_file(url, model_path.string())) {
        // İndirme sonrası tekrar boyut kontrolü
        size_t size = fs::file_size(model_path);
        if (size > min_size_bytes) {
            spdlog::info("✅ Download complete: {}", filename);
            return model_path.string();
        } else {
            spdlog::error("❌ Downloaded file is too small ({} bytes). Invalid URL or LFS pointer.", size);
            fs::remove(model_path); // Bozuk dosyayı temizle
            throw std::runtime_error("Download failed validation: " + filename);
        }
    } else {
        spdlog::error("❌ Failed to download: {}", filename);
        throw std::runtime_error("Download failed: " + filename);
    }
}

bool ModelManager::download_file(const std::string& url, const std::string& filepath) {
    // Curl komutu (Redirectleri takip et -L, Hata verirse çık -f)
    std::string command = "curl -L -f -o \"" + filepath + "\" \"" + url + "\"";
    spdlog::debug("Executing: {}", command);
    
    int result = std::system(command.c_str());
    return (result == 0) && fs::exists(filepath);
}
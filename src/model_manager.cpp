#include "model_manager.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <cstdlib>
#include <fstream>

namespace fs = std::filesystem;

std::string ModelManager::ensure_model(const Settings& settings) {
    fs::path model_dir(settings.model_dir);
    fs::path model_path = model_dir / settings.model_filename;

    // 1. Dizin kontrolü
    if (!fs::exists(model_dir)) {
        spdlog::info("Creating model directory: {}", settings.model_dir);
        fs::create_directories(model_dir);
    }

    // 2. Dosya kontrolü
    if (fs::exists(model_path)) {
        // Basit boyut kontrolü (boş dosya olmasın)
        if (fs::file_size(model_path) > 1024 * 1024) { // 1MB'dan büyükse geçerli say
            spdlog::info("✅ Model found at: {}", model_path.string());
            return model_path.string();
        } else {
            spdlog::warn("⚠️ Corrupted or empty model file found. Deleting and re-downloading...");
            fs::remove(model_path);
        }
    }

    // 3. URL Oluşturma
    // "ggml-base.bin" -> "base" ismini çıkar (URL şablonu için)
    std::string model_name = settings.model_filename;
    if (model_name.rfind("ggml-", 0) == 0) model_name.replace(0, 5, ""); // "ggml-" sil
    if (model_name.length() >= 4 && model_name.substr(model_name.length() - 4) == ".bin") 
        model_name = model_name.substr(0, model_name.length() - 4); // ".bin" sil

    std::string url = settings.model_url_template;
    // Basit string replace (placeholder varsa)
    std::string placeholder = "{model_name}";
    size_t pos = url.find(placeholder);
    if (pos != std::string::npos) {
        url.replace(pos, placeholder.length(), model_name);
    } else {
        // Placeholder yoksa ve dosya adı base değilse, belki manuel URL verilmiştir?
        // Varsayılan olarak URL'i olduğu gibi kullanırız, belki direkt link verilmiştir.
    }

    // Eğer URL template hala şablon gibiyse ama model adı uymuyorsa fallback
    // Örn: dosya adı "custom.bin", url template "{model_name}" -> url "custom".
    
    spdlog::info("⬇️ Model not found locally. Downloading from: {}", url);
    
    if (download_file(url, model_path.string())) {
        spdlog::info("✅ Model downloaded successfully to: {}", model_path.string());
        return model_path.string();
    } else {
        spdlog::error("❌ Failed to download model.");
        throw std::runtime_error("Model download failed");
    }
}

bool ModelManager::download_file(const std::string& url, const std::string& filepath) {
    // Robust indirme için 'curl' komutunu sistem üzerinden çağırıyoruz.
    // C++ kütüphaneleri (httplib) ile büyük dosya indirmek ve progress bar göstermek karmaşıktır.
    // Docker imajımızda 'curl' zaten var.
    
    std::string command = "curl -L -f -o \"" + filepath + "\" \"" + url + "\"";
    spdlog::debug("Executing: {}", command);
    
    int result = std::system(command.c_str());
    
    return (result == 0) && fs::exists(filepath) && (fs::file_size(filepath) > 0);
}
#include "model_manager.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

std::string ModelManager::ensure_model(const Settings& settings) {
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

    return ensure_file(settings.model_dir, settings.model_filename, url, 1024 * 1024);
}

std::string ModelManager::ensure_vad_model(const Settings& settings) {
    if (!settings.enable_vad) return "";
    return ensure_file(settings.model_dir, settings.vad_model_filename, settings.vad_model_url, 100 * 1024);
}

std::string ModelManager::ensure_file(const std::string& dir, const std::string& filename, const std::string& url, size_t min_size_bytes) {
    fs::path model_dir(dir);
    fs::path model_path = model_dir / filename;

    if (!fs::exists(model_dir)) {
        spdlog::info("Creating directory: {}", dir);
        fs::create_directories(model_dir);
    }

    if (fs::exists(model_path)) {
        size_t size = fs::file_size(model_path);
        if (size > min_size_bytes) {
            spdlog::info("✅ File ready: {} ({:.2f} MB)", filename, (double)size / (1024.0 * 1024.0));
            return model_path.string();
        } else {
            spdlog::warn("⚠️ Corrupted/Small file detected ({} bytes). Deleting...", size);
            fs::remove(model_path);
        }
    }

    spdlog::info("⬇️ Downloading: {} from {}", filename, url);
    
    if (download_file(url, model_path.string())) {
        size_t size = fs::file_size(model_path);
        if (size > min_size_bytes) {
            spdlog::info("✅ Download complete: {}", filename);
            return model_path.string();
        } else {
            spdlog::error("❌ Downloaded file is too small ({} bytes).", size);
            fs::remove(model_path); 
            throw std::runtime_error("Download failed validation: " + filename);
        }
    } else {
        spdlog::error("❌ Failed to download: {}", filename);
        throw std::runtime_error("Download failed: " + filename);
    }
}

bool ModelManager::download_file(const std::string& url, const std::string& filepath) {
    // GÜVENLİK DÜZELTMESİ: system() yerine fork() + execvp() kullanımı.
    // Bu yöntem shell injection saldırılarını engeller.
    
    pid_t pid = fork();

    if (pid == -1) {
        spdlog::error("Fork failed");
        return false;
    }

    if (pid == 0) {
        // Child Process
        // curl -L -f -o <filepath> <url>
        std::vector<char*> args;
        args.push_back(const_cast<char*>("curl"));
        args.push_back(const_cast<char*>("-L")); // Redirect takip et
        args.push_back(const_cast<char*>("-f")); // Hata durumunda fail ol (404 vs)
        args.push_back(const_cast<char*>("-o"));
        args.push_back(const_cast<char*>(filepath.c_str()));
        args.push_back(const_cast<char*>(url.c_str()));
        args.push_back(nullptr);

        // stdout/stderr'i susturmak isterseniz burayı açabilirsiniz:
        // freopen("/dev/null", "w", stdout);
        // freopen("/dev/null", "w", stderr);

        execvp("curl", args.data());
        
        // execvp sadece hata olursa buraya düşer
        std::perror("execvp failed");
        std::exit(1);
    } else {
        // Parent Process
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code == 0) {
                return fs::exists(filepath);
            } else {
                spdlog::error("Curl exited with code: {}", exit_code);
                return false;
            }
        } else {
            return false;
        }
    }
}
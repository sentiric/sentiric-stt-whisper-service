#pragma once

#include "config.h"
#include <string>

class ModelManager {
public:
    // Modeli kontrol eder, yoksa indirir.
    // Döndürdüğü değer: Kullanıma hazır modelin tam dosya yolu.
    static std::string ensure_model(const Settings& settings);

private:
    static bool download_file(const std::string& url, const std::string& filepath);
};
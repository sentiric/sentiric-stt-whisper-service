#include "speaker_cluster.h"
#include <cmath>

float SpeakerClusterer::cosine(const std::vector<float>& a, const std::vector<float>& b) {
    float dot = 0, normA = 0, normB = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    if (normA == 0 || normB == 0) return 0;
    return dot / (std::sqrt(normA) * std::sqrt(normB));
}

SpeakerClusterer::SpeakerClusterer(float threshold) : threshold_(threshold) {}

std::string SpeakerClusterer::assign_or_add(const std::vector<float>& vec) {
    std::string best_id;
    float best_sim = 0.0f;
    for (auto& [id, cls] : clusters_) {
        float sim = cosine(vec, cls.centroid);
        if (sim > best_sim) { best_sim = sim; best_id = id; }
    }
    if (!best_id.empty() && best_sim >= threshold_) {
        auto& cls = clusters_[best_id];
        for (size_t i = 0; i < cls.centroid.size(); ++i)
            cls.centroid[i] = (cls.centroid[i] * cls.count + vec[i]) / (cls.count + 1);
        cls.count++;
        return best_id;
    }
    std::string new_id = "spk_" + std::to_string(next_id_++);
    clusters_[new_id] = { new_id, vec, 1 };
    return new_id;
}
#pragma once
#include <vector>
#include <string>
#include <unordered_map>

struct SpeakerCluster {
    std::string id;               // spk_0, spk_1, ...
    std::vector<float> centroid;  // 8-D mean
    size_t count = 0;             // kaç segment
};

class SpeakerClusterer {
public:
    SpeakerClusterer(float threshold = 0.85f); // cosine threshold
    std::string assign_or_add(const std::vector<float>& vec);
    const std::unordered_map<std::string, SpeakerCluster>& clusters() const { return clusters_; }
private:
    float threshold_;
    std::unordered_map<std::string, SpeakerCluster> clusters_;
    int next_id_ = 0;
    float cosine(const std::vector<float>& a, const std::vector<float>& b);
};
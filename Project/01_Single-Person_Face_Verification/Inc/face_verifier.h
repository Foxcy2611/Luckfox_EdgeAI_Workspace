#pragma once
#include <string>
#include <vector>
class FaceVerifier {
public:
    explicit FaceVerifier(const std::string& reference_path);
    float similarity(std::vector<float> current) const;
    bool accept(float score,float threshold) const { return score >= threshold; }
private:
    std::vector<float> reference_;
};

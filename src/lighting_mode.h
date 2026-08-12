#pragma once
#include <string>

enum class LightingMode {
    BlinnPhong,
    PhotonMapping
};

inline std::string lighting_mode_to_string(LightingMode mode) {
    switch (mode) {
        case LightingMode::BlinnPhong: return "blinn-phong";
        case LightingMode::PhotonMapping: return "photon-mapping";
        default: return "unknown";
    }
}

inline LightingMode string_to_lighting_mode(const std::string& str) {
    if (str == "blinn-phong") return LightingMode::BlinnPhong;
    if (str == "photon-mapping") return LightingMode::PhotonMapping;
    return LightingMode::PhotonMapping;  // Default
}
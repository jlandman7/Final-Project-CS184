// image_output.cpp
#include "image_output.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <filesystem>


#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace fs = std::filesystem;

// In image_output.cpp, temporarily bypass tone mapping
std::vector<uint8_t> ImageOutput::tone_map_to_ldr(const std::vector<glm::vec4>& hdr_pixels,
                                                   int width, int height) {
    std::vector<uint8_t> ldr_pixels(width * height * 4);

    // Debug: just output raw HDR as 8-bit to see what we're reading
    for (size_t i = 0; i < hdr_pixels.size(); ++i) {
        ldr_pixels[i * 4 + 0] = static_cast<uint8_t>(glm::clamp(hdr_pixels[i].r, 0.0f, 1.0f) * 255.0f);
        ldr_pixels[i * 4 + 1] = static_cast<uint8_t>(glm::clamp(hdr_pixels[i].g, 0.0f, 1.0f) * 255.0f);
        ldr_pixels[i * 4 + 2] = static_cast<uint8_t>(glm::clamp(hdr_pixels[i].b, 0.0f, 1.0f) * 255.0f);
        ldr_pixels[i * 4 + 3] = 255;
    }

    return ldr_pixels;
}

// std::vector<uint8_t> ImageOutput::tone_map_to_ldr(const std::vector<glm::vec4>& hdr_pixels,
//                                                    int width, int height) {
//     std::vector<uint8_t> ldr_pixels(width * height * 4);

//     for (size_t i = 0; i < hdr_pixels.size(); ++i) {
//         glm::vec3 color(hdr_pixels[i].r, hdr_pixels[i].g, hdr_pixels[i].b);

//         // Simple tone mapping: reinhard
//         color = color / (color + glm::vec3(1.0f));

//         // Gamma correction
//         color = glm::pow(color, glm::vec3(1.0f / 2.2f));

//         // Clamp and convert to 8-bit
//         ldr_pixels[i * 4 + 0] = static_cast<uint8_t>(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f);
//         ldr_pixels[i * 4 + 1] = static_cast<uint8_t>(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f);
//         ldr_pixels[i * 4 + 2] = static_cast<uint8_t>(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f);
//         ldr_pixels[i * 4 + 3] = 255;
//     }

//     return ldr_pixels;
// }

void ImageOutput::write_png(const std::string& filepath,
                           const std::vector<uint8_t>& ldr_pixels,
                           int width, int height) {
    auto dir = fs::path(filepath).parent_path();
    if (!dir.empty() && !fs::exists(dir)) {
        fs::create_directories(dir);
        std::cout << "Created directory: " << dir << "\n";
    }

    std::cout << "Writing PNG: " << filepath << "\n";
    int result = stbi_write_png(filepath.c_str(), width, height, 4, ldr_pixels.data(), width * 4);
    if (result == 0) {
        throw std::runtime_error("Failed to write PNG: " + filepath);
    }
    std::cout << "  Success!\n";
}
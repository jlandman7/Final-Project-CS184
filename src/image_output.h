// image_output.h
#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

class ImageOutput {
public:
    // Tone map and gamma correct HDR to LDR
    static std::vector<uint8_t> tone_map_to_ldr(const std::vector<glm::vec4>& hdr_pixels,
                                                 int width, int height);
    
    // Write LDR pixels to PNG
    static void write_png(const std::string& filepath,
                         const std::vector<uint8_t>& ldr_pixels,
                         int width, int height);
};
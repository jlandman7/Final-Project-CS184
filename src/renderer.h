// renderer.h
#pragma once
#include "framebuffer.h"
#include "cornell_box.h"
#include <glm/glm.hpp>
#include <OpenGL/gl3.h>
#include <memory>
#include <vector>

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    // Initialize (loads shaders, etc.)
    void init();

    // Render a single frame
    void render_frame(float time);

    // Get the rendered color for output
    void read_color_to_cpu(std::vector<glm::vec4>& out_pixels) const {
        framebuffer->read_color_to_cpu(out_pixels);
    }

    // Access for debugging
    const CornellBox& get_scene() const { return cornell_box; }
    const Framebuffer& get_framebuffer() const { return *framebuffer; }

private:
    std::unique_ptr<Framebuffer> framebuffer;
    CornellBox cornell_box;

    GLuint shader_program;
    glm::mat4 projection, view;
    glm::vec3 camera_pos;

    void load_shaders();
    void setup_camera();
};
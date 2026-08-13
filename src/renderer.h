#pragma once
#include "framebuffer.h"
#include "cornell_box.h"
#include <glm/glm.hpp>
#include <OpenGL/gl3.h>
#include "water_mesh.h"
#include <memory>
#include <vector>

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    void render_to_screen(int window_fb_width, int window_fb_height);
    void init();
    void render_frame(float time);

    void read_color_to_cpu(std::vector<glm::vec4>& out_pixels) const {
        framebuffer->read_color_to_cpu(out_pixels);
    }

    void upload_cpu_buffer(const std::vector<glm::vec4>& buffer) const;

    const CornellBox& get_scene() const { return cornell_box; }
    const Framebuffer& get_framebuffer() const { return *framebuffer; }

    glm::vec3 get_camera_position() const;
    glm::mat4 get_view_matrix() const;
    glm::mat4 get_projection_matrix() const;

    void set_water_mesh(WaterMesh* mesh) { water_mesh = mesh; }

private:
    std::unique_ptr<Framebuffer> framebuffer;
    CornellBox cornell_box;
    WaterMesh* water_mesh;

    GLuint display_shader_program = 0;
    GLuint screen_vao = 0;
    void init_screen_quad();

    GLuint shader_program;
    glm::mat4 projection, view;
    glm::vec3 camera_pos;

    void load_shaders();
    void setup_camera();
    void render_scene();  // Common rendering logic
};
// framebuffer.h
#pragma once
#include <glm/glm.hpp>

// Platform-specific OpenGL headers
#ifdef _WIN32
    #include <GL/gl.h>
#else
    #include <OpenGL/gl3.h>
#endif

#include <memory>

class Framebuffer {
public:
    Framebuffer(int width, int height);
    ~Framebuffer();

    // Bind for rendering
    void bind() const;
    void unbind() const;

    // Get FBO handle
    GLuint get_fbo() const { return fbo; }

    // Get texture handles for reading/sampling
    GLuint get_color_texture() const { return color_texture; }
    GLuint get_normal_texture() const { return normal_texture; }
    GLuint get_position_texture() const { return position_texture; }
    GLuint get_depth_texture() const { return depth_texture; }

    // Dimensions
    int get_width() const { return width; }
    int get_height() const { return height; }

    // Read framebuffer to CPU (for PNG output)
    void read_color_to_cpu(std::vector<glm::vec4>& out_pixels) const;

private:
    GLuint fbo;
    GLuint color_texture;      // HDR: GL_RGBA32F
    GLuint normal_texture;     // Normals: GL_RGBA16F (A unused, reserve for later)
    GLuint position_texture;   // World positions: GL_RGBA32F
    GLuint depth_texture;      // Depth: GL_DEPTH_COMPONENT32F
    GLuint depth_rbo;          // Renderbuffer for depth (optional: could use texture)

    int width, height;

    void create_textures();
    void create_fbo();
    void cleanup();
};
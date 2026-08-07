// renderer.cpp
#include "renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace {
    std::string read_shader_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open shader file: " + path);
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    GLuint compile_shader(const std::string& source, GLenum type) {
        GLuint shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        int success;
        char info_log[512];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 512, nullptr, info_log);
            throw std::runtime_error(std::string("Shader compilation failed: ") + info_log);
        }
        return shader;
    }

    GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
        GLuint program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);

        int success;
        char info_log[512];
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(program, 512, nullptr, info_log);
            throw std::runtime_error(std::string("Program linking failed: ") + info_log);
        }

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return program;
    }
}

Renderer::Renderer(int width, int height) 
    : shader_program(0), camera_pos(0.5f, 0.5f, 1.5f) {
    framebuffer = std::make_unique<Framebuffer>(width, height);
}

Renderer::~Renderer() {
    if (shader_program) glDeleteProgram(shader_program);
}

void Renderer::init() {
    cornell_box.generate_procedural();
    load_shaders();
    setup_camera();
}

void Renderer::load_shaders() {
    // For now, embed shaders as strings (will move to files later)
    std::string vertex_source = R"glsl(
        #version 410 core
        layout (location = 0) in vec3 position;
        layout (location = 1) in vec3 normal;

        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 model;

        out VS_OUT {
            vec3 position;
            vec3 normal;
        } vs_out;

        void main() {
            gl_Position = projection * view * model * vec4(position, 1.0);
            vs_out.position = (model * vec4(position, 1.0)).xyz;
            vs_out.normal = normalize(mat3(model) * normal);
        }
    )glsl";

    std::string fragment_source = R"glsl(
        #version 410 core
        layout (location = 0) out vec4 out_color;
        layout (location = 1) out vec4 out_normal;
        layout (location = 2) out vec4 out_position;

        in VS_OUT {
            vec3 position;
            vec3 normal;
        } fs_in;

        void main() {
            vec3 normal = normalize(fs_in.normal);
            
            // Visualize normals as RGB colors
            vec3 color = normal * 0.5 + 0.5;
            
            out_color = vec4(color, 1.0);
            out_normal = vec4(normal, 1.0);
            out_position = vec4(fs_in.position, 1.0);
        }
    )glsl";

    GLuint vs = compile_shader(vertex_source, GL_VERTEX_SHADER);
    GLuint fs = compile_shader(fragment_source, GL_FRAGMENT_SHADER);
    shader_program = link_program(vs, fs);

    // Debug: verify shader uniforms
    std::cout << "Shader program ID: " << shader_program << "\n";
    std::cout << "Uniform locations:\n";
    std::cout << "  projection: " << glGetUniformLocation(shader_program, "projection") << "\n";
    std::cout << "  view: " << glGetUniformLocation(shader_program, "view") << "\n";
    std::cout << "  model: " << glGetUniformLocation(shader_program, "model") << "\n";
    std::cout << "  light_dir: " << glGetUniformLocation(shader_program, "light_dir") << "\n";
    std::cout << "  camera_pos: " << glGetUniformLocation(shader_program, "camera_pos") << "\n";
}

void Renderer::setup_camera() {
    float aspect = framebuffer->get_width() / float(framebuffer->get_height());
    projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    view = glm::lookAt(camera_pos, glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0, 1, 0));
}

void Renderer::render_frame(float time) {
    framebuffer->bind();

    // Use a very obvious clear color to test
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);  // Bright red
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glUseProgram(shader_program);

    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, glm::value_ptr(model));

    cornell_box.render();

    glUseProgram(0);
    framebuffer->unbind();
}
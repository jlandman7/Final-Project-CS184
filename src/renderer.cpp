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

        uniform vec3 light_dir;
        uniform vec3 camera_pos;
        
        const vec3 light_color = vec3(1.0, 1.0, 1.0);
        const float ambient_strength = 0.3;
        const float diffuse_strength = 0.6;
        const float specular_strength = 0.3;
        const float shininess = 32.0;

        void main() {
            vec3 normal = normalize(fs_in.normal);
            
            // Ambient
            vec3 ambient = ambient_strength * light_color;
            
            // Diffuse
            float diff = max(dot(normal, light_dir), 0.0);
            vec3 diffuse = diffuse_strength * diff * light_color;
            
            // Specular (Blinn-Phong)
            vec3 view_dir = normalize(camera_pos - fs_in.position);
            vec3 halfway = normalize(light_dir + view_dir);
            float spec = pow(max(dot(normal, halfway), 0.0), shininess);
            vec3 specular = specular_strength * spec * light_color;
            
            vec3 color = (ambient + diffuse + specular);
            
            out_color = vec4(color, 1.0);
            out_normal = vec4(normal, 1.0);
            out_position = vec4(fs_in.position, 1.0);
        }
    )glsl";

    GLuint vs = compile_shader(vertex_source, GL_VERTEX_SHADER);
    GLuint fs = compile_shader(fragment_source, GL_FRAGMENT_SHADER);
    shader_program = link_program(vs, fs);
}

void Renderer::setup_camera() {
    float aspect = framebuffer->get_width() / float(framebuffer->get_height());
    projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    view = glm::lookAt(camera_pos, glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0, 1, 0));
}

void Renderer::render_frame(float time) {
    framebuffer->bind();

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    // Temporarily disable culling to debug
    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_BACK);

    glUseProgram(shader_program);

    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, glm::value_ptr(model));
    
    glm::vec3 light_dir = glm::normalize(glm::vec3(0.5f, 1.0f, 0.5f));
    glUniform3fv(glGetUniformLocation(shader_program, "light_dir"), 1, glm::value_ptr(light_dir));
    glUniform3fv(glGetUniformLocation(shader_program, "camera_pos"), 1, glm::value_ptr(camera_pos));

    cornell_box.render();

    glUseProgram(0);
    glDisable(GL_CULL_FACE);
    framebuffer->unbind();
}
// renderer.cpp
#include "renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace {
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
    : shader_program(0) {
    framebuffer = std::make_unique<Framebuffer>(width, height);
}

Renderer::~Renderer() {
    if (shader_program) glDeleteProgram(shader_program);
}

void Renderer::init() {
    cornell_box.generate_procedural();
    load_shaders();
    init_screen_quad();
    setup_camera();
}

void Renderer::load_shaders() {
    std::string vertex_source = R"glsl(
        #version 410 core
        layout (location = 0) in vec3 position;
        layout (location = 1) in vec3 normal;
        layout (location = 2) in vec3 color;

        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 model;

        out VS_OUT {
            vec3 position;
            vec3 normal;
            vec3 color;
        } vs_out;

        void main() {
            gl_Position = projection * view * model * vec4(position, 1.0);
            vs_out.position = (model * vec4(position, 1.0)).xyz;
            vs_out.normal = normalize(mat3(model) * normal);
            vs_out.color = color;
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
            vec3 color;
        } fs_in;

        uniform vec3 camera_pos;

        void main() {
            vec3 normal = normalize(fs_in.normal);

            if (fs_in.color.r > 2.0) {
                out_color = vec4(fs_in.color, 1.0);
                out_normal = vec4(normal, 1.0);
                out_position = vec4(fs_in.position, 1.0);
                return;
            }

            vec3 light_pos = vec3(0.5, 0.98, 0.5);
            vec3 light_dir = light_pos - fs_in.position;
            float dist = length(light_dir);
            light_dir = normalize(light_dir);

            float attenuation = 1.0 / (1.0 + 0.5 * dist + 1.0 * dist * dist);
            vec3 ambient = 0.1 * fs_in.color;
            float diff = max(dot(normal, light_dir), 0.0);
            vec3 diffuse = diff * fs_in.color * attenuation * 2.0;
            vec3 result = ambient + diffuse;

            out_color = vec4(result, 1.0);
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
    this->projection = glm::perspective(glm::radians(38.0f), aspect, 0.1f, 100.0f);
    
    this->camera_pos = glm::vec3(0.5f, 0.60f, 2.30f);
    glm::vec3 target = glm::vec3(0.5f, 0.40f, 0.50f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    
    this->view = glm::lookAt(camera_pos, target, up);
}

void Renderer::render_frame(float time) {
    framebuffer->bind();

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
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
    
    glm::vec3 light_dir = glm::normalize(glm::vec3(0.5f, 1.0f, 0.5f));
    glUniform3fv(glGetUniformLocation(shader_program, "light_dir"), 1, glm::value_ptr(light_dir));
    glUniform3fv(glGetUniformLocation(shader_program, "camera_pos"), 1, glm::value_ptr(camera_pos));

    cornell_box.render();

    if (water_mesh) {
        water_mesh->render();
    }

    glUseProgram(0);
    framebuffer->unbind();
}

void Renderer::init_screen_quad() {
    float screen_vertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    GLuint vbo;
    glGenVertexArrays(1, &screen_vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(screen_vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screen_vertices), screen_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    std::string vs_src = R"glsl(
        #version 410 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            TexCoord = aTexCoord;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )glsl";

    std::string fs_src = R"glsl(
        #version 410 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D hdrBuffer;

        void main() {
            vec3 hdrColor = texture(hdrBuffer, TexCoord).rgb;
            vec3 mapped = hdrColor / (hdrColor + vec3(1.0));
            mapped = pow(mapped, vec3(1.0 / 2.2));
            FragColor = vec4(mapped, 1.0);
        }
    )glsl";

    GLuint vs = compile_shader(vs_src, GL_VERTEX_SHADER);
    GLuint fs = compile_shader(fs_src, GL_FRAGMENT_SHADER);
    display_shader_program = link_program(vs, fs);
}

void Renderer::render_to_screen(int window_fb_width, int window_fb_height) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_fb_width, window_fb_height);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(display_shader_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, framebuffer->get_color_texture());
    glUniform1i(glGetUniformLocation(display_shader_program, "hdrBuffer"), 0);

    glBindVertexArray(screen_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glUseProgram(0);
}

glm::vec3 Renderer::get_camera_position() const { return camera_pos; }
glm::mat4 Renderer::get_view_matrix() const { return view; }
glm::mat4 Renderer::get_projection_matrix() const { return projection; }

void Renderer::upload_cpu_buffer(const std::vector<glm::vec4>& buffer) const {
    glBindTexture(GL_TEXTURE_2D, framebuffer->get_color_texture());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                    framebuffer->get_width(), framebuffer->get_height(), 
                    GL_RGBA, GL_FLOAT, buffer.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}
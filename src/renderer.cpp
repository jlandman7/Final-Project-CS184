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
	framebuffer =
		std::make_unique<Framebuffer>(width, height);

	scene_framebuffer =
		std::make_unique<Framebuffer>(width, height);
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
uniform int material_type;

uniform sampler2D scene_color;
uniform vec2 screen_resolution;

uniform mat4 projection;
uniform mat4 view;

void main() {
    vec3 N = normalize(fs_in.normal);

    // -----------------------------
    // WATER
    // -----------------------------
    if (material_type == 1) {

    vec3 N = normalize(fs_in.normal);
    vec3 V = normalize(camera_pos - fs_in.position);

    // --------------------------------
    // Fresnel
    // --------------------------------

    float eta_air   = 1.0;
    float eta_water = 1.333;

    float F0 =
        (eta_air - eta_water) /
        (eta_air + eta_water);

    F0 *= F0;

    float cosTheta =
        clamp(dot(N, V), 0.0, 1.0);

    float fresnel =
        F0 +
        (1.0 - F0) *
        pow(1.0 - cosTheta, 5.0);

    // --------------------------------
    // Refraction direction
    // --------------------------------

    float eta = eta_air / eta_water;

    vec3 refracted =
        refract(-V, N, eta);

    // --------------------------------
    // Screen-space refraction
    // --------------------------------

    // Move a short distance along the refracted ray.
    // This approximates where the ray would hit the
    // background scene.
    vec3 sample_world =
        fs_in.position + refracted * 0.15;

    vec4 sample_clip =
        projection *
        view *
        vec4(sample_world, 1.0);

    vec2 refract_uv =
        sample_clip.xy / sample_clip.w;

    // NDC [-1,1] -> texture [0,1]
    refract_uv =
        refract_uv * 0.5 + 0.5;

    refract_uv =
        clamp(refract_uv,
              vec2(0.001),
              vec2(0.999));

    vec3 refracted_color =
        texture(scene_color, refract_uv).rgb;

    // --------------------------------
    // Slight water absorption/tint
    // --------------------------------

    vec3 water_tint =
        vec3(0.85, 0.96, 1.0);

    refracted_color *= water_tint;

    // --------------------------------
    // Cheap reflected contribution
    // --------------------------------

    vec3 R = reflect(-V, N);

    vec3 reflection_color =
        mix(
            vec3(0.08, 0.10, 0.12),
            vec3(0.80, 0.90, 1.0),
            clamp(R.y, 0.0, 1.0)
        );

    // Fresnel:
    // perpendicular -> refraction
    // grazing       -> reflection
    vec3 result =
        mix(
            refracted_color,
            reflection_color,
            fresnel
        );

    // --------------------------------
    // Light highlight
    // --------------------------------

    vec3 light_pos =
        vec3(0.5, 0.98, 0.5);

    vec3 L =
        normalize(light_pos - fs_in.position);

    vec3 H =
        normalize(L + V);

    float spec =
        pow(
            max(dot(N, H), 0.0),
            128.0
        );

    result +=
        vec3(1.0) * spec * 1.5;

    out_color =
        vec4(result, 1.0);

    out_normal =
        vec4(N, 1.0);

    out_position =
        vec4(fs_in.position, 1.0);

    return;
}

    // -----------------------------
    // CORNELL BOX
    // -----------------------------

    if (fs_in.color.r > 2.0) {
        out_color = vec4(fs_in.color, 1.0);
        out_normal = vec4(N, 1.0);
        out_position = vec4(fs_in.position, 1.0);
        return;
    }

    vec3 light_pos = vec3(0.5, 0.98, 0.5);
    vec3 light_dir = light_pos - fs_in.position;

    float dist = length(light_dir);
    light_dir = normalize(light_dir);

    float attenuation =
        1.0 /
        (1.0 + 0.5 * dist + dist * dist);

    vec3 ambient =
        0.1 * fs_in.color;

    float diff =
        max(dot(N, light_dir), 0.0);

    vec3 diffuse =
        diff *
        fs_in.color *
        attenuation *
        2.0;

    vec3 result =
        ambient + diffuse;

    out_color = vec4(result, 1.0);
    out_normal = vec4(N, 1.0);
    out_position = vec4(fs_in.position, 1.0);
}
    )glsl";

	GLuint vs = compile_shader(vertex_source, GL_VERTEX_SHADER);
	GLuint fs = compile_shader(fragment_source, GL_FRAGMENT_SHADER);
	shader_program = link_program(vs, fs);
}

void Renderer::setup_camera() {
	float aspect = framebuffer->get_width() / float(framebuffer->get_height());

	// Narrower FOV (~38 degrees) prevents wide-angle edge distortion 
	// and mimics a cinematic lens framing the box neatly.
	projection = glm::perspective(glm::radians(38.0f), aspect, 0.1f, 100.0f);

	// glm::vec3 camera_pos = glm::vec3(0.5f, 0.70f, 2.20f);
	// glm::vec3 target = glm::vec3(0.5f, 0.38f, 0.45f);

	//glm::vec3 camera_pos = glm::vec3(0.5f, 0.60f, 2.30f);
	//glm::vec3 target = glm::vec3(0.5f, 0.40f, 0.50f);

	camera_pos = glm::vec3(0.5f, 0.85f, 2.30f);
	glm::vec3 target = glm::vec3(0.5f, 0.30f, 0.50f);

	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

	view = glm::lookAt(camera_pos, target, up);
}

void Renderer::render_frame(float time) {
	glm::mat4 model(1.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glUseProgram(shader_program);

	// Common uniform locations
	GLint projectionLoc =
		glGetUniformLocation(shader_program, "projection");

	GLint viewLoc =
		glGetUniformLocation(shader_program, "view");

	GLint modelLoc =
		glGetUniformLocation(shader_program, "model");

	GLint cameraLoc =
		glGetUniformLocation(shader_program, "camera_pos");

	GLint materialLoc =
		glGetUniformLocation(shader_program, "material_type");

	// ============================================
	// PASS 1: Render Cornell box WITHOUT water
	// ============================================

	scene_framebuffer->bind();

	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(shader_program);

	glUniformMatrix4fv(
		projectionLoc,
		1,
		GL_FALSE,
		glm::value_ptr(projection)
	);

	glUniformMatrix4fv(
		viewLoc,
		1,
		GL_FALSE,
		glm::value_ptr(view)
	);

	glUniformMatrix4fv(
		modelLoc,
		1,
		GL_FALSE,
		glm::value_ptr(model)
	);

	glUniform3fv(
		cameraLoc,
		1,
		glm::value_ptr(camera_pos)
	);

	// Cornell box material
	glUniform1i(materialLoc, 0);

	cornell_box.render();

	scene_framebuffer->unbind();


	// ============================================
	// Copy scene framebuffer -> final framebuffer
	// ============================================

	glBindFramebuffer(
		GL_READ_FRAMEBUFFER,
		scene_framebuffer->get_fbo()
	);

	glBindFramebuffer(
		GL_DRAW_FRAMEBUFFER,
		framebuffer->get_fbo()
	);

	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	glBlitFramebuffer(
		0,
		0,
		scene_framebuffer->get_width(),
		scene_framebuffer->get_height(),

		0,
		0,
		framebuffer->get_width(),
		framebuffer->get_height(),

		GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
		GL_NEAREST
	);


	// ============================================
	// PASS 2: Render water over copied scene
	// ============================================

	framebuffer->bind();

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glUseProgram(shader_program);

	// Restore all three MRT outputs.
	// glDrawBuffer above changed this temporarily.
	GLenum draw_buffers[] = {
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2
	};

	glDrawBuffers(3, draw_buffers);

	glUniformMatrix4fv(
		projectionLoc,
		1,
		GL_FALSE,
		glm::value_ptr(projection)
	);

	glUniformMatrix4fv(
		viewLoc,
		1,
		GL_FALSE,
		glm::value_ptr(view)
	);

	glUniformMatrix4fv(
		modelLoc,
		1,
		GL_FALSE,
		glm::value_ptr(model)
	);

	glUniform3fv(
		cameraLoc,
		1,
		glm::value_ptr(camera_pos)
	);

	// Water material
	glUniform1i(materialLoc, 1);

	// --------------------------------------------
	// Give water shader the scene-only image
	// --------------------------------------------

	glActiveTexture(GL_TEXTURE0);

	glBindTexture(
		GL_TEXTURE_2D,
		scene_framebuffer->get_color_texture()
	);

	glUniform1i(
		glGetUniformLocation(
			shader_program,
			"scene_color"
		),
		0
	);

	// Render water
	if (water_mesh) {
		water_mesh->render();
	}

	// Cleanup
	glBindTexture(GL_TEXTURE_2D, 0);

	glUseProgram(0);

	framebuffer->unbind();
}

void Renderer::init_screen_quad() {
	// 6-vertex standard quad covering NDC [-1, 1]
	float screen_vertices[] = {
		// Position    // TexCoords
		-1.0f,  1.0f,  0.0f, 1.0f, // Top-Left
		-1.0f, -1.0f,  0.0f, 0.0f, // Bottom-Left
		 1.0f, -1.0f,  1.0f, 0.0f, // Bottom-Right

		-1.0f,  1.0f,  0.0f, 1.0f, // Top-Left
		 1.0f, -1.0f,  1.0f, 0.0f, // Bottom-Right
		 1.0f,  1.0f,  1.0f, 1.0f  // Top-Right
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
            
            // Reinhard tone mapping
            vec3 mapped = hdrColor / (hdrColor + vec3(1.0));
            
            // Gamma correction
            mapped = pow(mapped, vec3(1.0 / 2.2));
            
            FragColor = vec4(mapped, 1.0);
        }
    )glsl";

	GLuint vs = compile_shader(vs_src, GL_VERTEX_SHADER);
	GLuint fs = compile_shader(fs_src, GL_FRAGMENT_SHADER);
	display_shader_program = link_program(vs, fs);
}

void Renderer::render_to_screen(int window_fb_width, int window_fb_height) {
	// Bind window framebuffer (0) and set viewport
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window_fb_width, window_fb_height);

	// Disable 3D depth testing and backface culling for full-screen pass
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
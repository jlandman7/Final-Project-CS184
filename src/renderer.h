#pragma once
#include "framebuffer.h"
#include "cornell_box.h"
#include <glm/glm.hpp>

// replaced for windows compatibility, uncomment if it no longer works on macOS

#include <glad/glad.h>

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

	const CornellBox& get_scene() const { return cornell_box; }
	const Framebuffer& get_framebuffer() const { return *framebuffer; }

	void set_water_mesh(WaterMesh* mesh) { water_mesh = mesh; }

private:
	// Final rendering framebuffer (HDR)
	std::unique_ptr<Framebuffer> framebuffer;
	std::unique_ptr<Framebuffer> scene_framebuffer;// scene without water
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
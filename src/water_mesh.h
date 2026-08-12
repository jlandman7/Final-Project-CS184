#pragma once
#include "water_simulation.h"
#include <glm/glm.hpp>


#include <glad/glad.h>

#include <vector>

class WaterMesh {
public:
	WaterMesh(const WaterSimulation& sim);
	~WaterMesh();

	// Update mesh from current simulation state
	void update();

	// Render the water surface
	void render() const;

private:
	const WaterSimulation& simulation;

	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<GLuint> indices;

	GLuint vao, vbo, ebo;

	void generate_mesh();
	void setup_buffers();
	void cleanup();
};
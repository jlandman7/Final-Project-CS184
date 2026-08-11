#include "water_mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

WaterMesh::WaterMesh(const WaterSimulation& sim)
    : simulation(sim), vao(0), vbo(0), ebo(0) {
    generate_mesh();
    setup_buffers();
}

WaterMesh::~WaterMesh() {
    cleanup();
}

void WaterMesh::generate_mesh() {
    int res = simulation.get_resolution();
    float domain = simulation.get_domain_size();
    float dx = domain / (res - 1);

    positions.clear();
    normals.clear();
    indices.clear();

    // Create vertex positions from height field
    for (int y = 0; y < res; ++y) {
        for (int x = 0; x < res; ++x) {
            float px = x * dx;
            float py = y * dx;
            float pz = simulation.get_height(x, y);

            positions.push_back(glm::vec3(px, pz, py));
            normals.push_back(glm::vec3(0, 1, 0));  // Placeholder, will update
        }
    }

    // Create quad faces
    for (int y = 0; y < res - 1; ++y) {
        for (int x = 0; x < res - 1; ++x) {
            int v0 = y * res + x;
            int v1 = y * res + (x + 1);
            int v2 = (y + 1) * res + (x + 1);
            int v3 = (y + 1) * res + x;

            // Two triangles per quad
            indices.push_back(v0);
            indices.push_back(v1);
            indices.push_back(v2);

            indices.push_back(v0);
            indices.push_back(v2);
            indices.push_back(v3);
        }
    }

    // Compute normals from surface
    update();
}

void WaterMesh::update() {
    int res = simulation.get_resolution();
    float domain = simulation.get_domain_size();
    float dx = domain / (res - 1);

    // Update positions and compute normals
    for (int y = 0; y < res; ++y) {
        for (int x = 0; x < res; ++x) {
            float px = x * dx;
            float py = y * dx;
            float pz = simulation.get_height(x, y);

            int idx = y * res + x;
            positions[idx] = glm::vec3(px, pz, py);

            // Boundary-safe height lookups for normals
            float h_left   = simulation.get_height(std::max(x - 1, 0), y);
            float h_right  = simulation.get_height(std::min(x + 1, res - 1), y);
            float h_top    = simulation.get_height(x, std::max(y - 1, 0));
            float h_bottom = simulation.get_height(x, std::min(y + 1, res - 1));

            glm::vec3 tangent_x(2.0f * dx, h_right - h_left, 0.0f);
            glm::vec3 tangent_z(0.0f, h_bottom - h_top, 2.0f * dx);

            normals[idx] = glm::normalize(glm::cross(tangent_z, tangent_x));
        }
    }

    // Single interleaved update
    std::vector<float> vertex_data;
    vertex_data.reserve(positions.size() * 6);
    for (size_t i = 0; i < positions.size(); ++i) {
        vertex_data.push_back(positions[i].x);
        vertex_data.push_back(positions[i].y);
        vertex_data.push_back(positions[i].z);
        vertex_data.push_back(normals[i].x);
        vertex_data.push_back(normals[i].y);
        vertex_data.push_back(normals[i].z);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_data.size() * sizeof(float), vertex_data.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void WaterMesh::setup_buffers() {
    // Interleaved: position, normal, position, normal, ...
    std::vector<float> vertex_data;
    for (size_t i = 0; i < positions.size(); ++i) {
        vertex_data.push_back(positions[i].x);
        vertex_data.push_back(positions[i].y);
        vertex_data.push_back(positions[i].z);
        vertex_data.push_back(normals[i].x);
        vertex_data.push_back(normals[i].y);
        vertex_data.push_back(normals[i].z);
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_data.size() * sizeof(float), vertex_data.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    glVertexAttrib3f(2, 0.2f, 0.5f, 0.8f);

    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void WaterMesh::render() const {
    glDisable(GL_CULL_FACE);  // Temporarily disable culling to test
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}

void WaterMesh::cleanup() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
}
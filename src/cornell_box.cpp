// cornell_box.cpp
#include "cornell_box.h"
#include <stdexcept>
#include <iostream>

CornellBox::CornellBox() : vao(0), vbo(0), ebo(0) {
}

CornellBox::~CornellBox() {
    cleanup();
}

void CornellBox::generate_procedural() {
    const float size = 1.0f;
    
    glm::vec3 p000(0, 0, 0);
    glm::vec3 p100(size, 0, 0);
    glm::vec3 p010(0, size, 0);
    glm::vec3 p110(size, size, 0);
    glm::vec3 p001(0, 0, size);
    glm::vec3 p101(size, 0, size);
    glm::vec3 p011(0, size, size);
    glm::vec3 p111(size, size, size);

    mesh.positions.clear();
    mesh.normals.clear();
    mesh.indices.clear();

    // Bottom face (y = 0)
    add_box_face(p000, p100, p110, p010);
    // Top face (y = size)
    add_box_face(p010, p110, p111, p011);
    // Back face (z = size)
    add_box_face(p001, p101, p111, p011);
    // Front face (z = 0)
    add_box_face(p000, p010, p110, p100);
    // Left face (x = 0)
    add_box_face(p000, p001, p011, p010);
    // Right face (x = size)
    add_box_face(p100, p110, p111, p101);

    setup_buffers();

    // Debug output
    std::cout << "Cornell Box Debug:\n";
    std::cout << "  Vertices: " << mesh.positions.size() << "\n";
    std::cout << "  Triangles: " << mesh.indices.size() / 3 << "\n";
    std::cout << "  First 3 vertices:\n";
    for (int i = 0; i < std::min(3, (int)mesh.positions.size()); ++i) {
        std::cout << "    [" << i << "] pos=(" << mesh.positions[i].x << "," 
                  << mesh.positions[i].y << "," << mesh.positions[i].z << ") norm=("
                  << mesh.normals[i].x << "," << mesh.normals[i].y << "," << mesh.normals[i].z << ")\n";
    }
}

void CornellBox::add_box_face(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3) {
    glm::vec3 edge1 = p1 - p0;
    glm::vec3 edge2 = p2 - p0;
    glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
    
    // Flip normal to point inward (we're inside the box)
    normal = -normal;

    GLuint base_index = mesh.positions.size();

    mesh.positions.push_back(p0);
    mesh.positions.push_back(p1);
    mesh.positions.push_back(p2);
    mesh.positions.push_back(p3);

    mesh.normals.push_back(normal);
    mesh.normals.push_back(normal);
    mesh.normals.push_back(normal);
    mesh.normals.push_back(normal);

    mesh.indices.push_back(base_index + 0);
    mesh.indices.push_back(base_index + 1);
    mesh.indices.push_back(base_index + 2);

    mesh.indices.push_back(base_index + 0);
    mesh.indices.push_back(base_index + 2);
    mesh.indices.push_back(base_index + 3);
}

void CornellBox::load_from_dae(const std::string& filepath) {
    // TODO: Use Assimp to load DAE
    // For now, just fall back to procedural
    std::cerr << "DAE loading not yet implemented, using procedural box\n";
    generate_procedural();
}

void CornellBox::setup_buffers() {
    if (vao == 0) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
    }

    glBindVertexArray(vao);

    // Position + Normal interleaved in single VBO
    std::vector<float> vertex_data;
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        vertex_data.push_back(mesh.positions[i].x);
        vertex_data.push_back(mesh.positions[i].y);
        vertex_data.push_back(mesh.positions[i].z);
        vertex_data.push_back(mesh.normals[i].x);
        vertex_data.push_back(mesh.normals[i].y);
        vertex_data.push_back(mesh.normals[i].z);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_data.size() * sizeof(float), vertex_data.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(GLuint), mesh.indices.data(), GL_STATIC_DRAW);

    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void CornellBox::render() const {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void CornellBox::cleanup() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
}
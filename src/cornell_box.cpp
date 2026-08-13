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
    mesh.positions.clear();
    mesh.normals.clear();
    mesh.colors.clear();
    mesh.indices.clear();

    auto add_quad = [this](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, 
                           glm::vec3 normal, glm::vec3 color) {
        GLuint base = static_cast<GLuint>(mesh.positions.size());
        mesh.positions.push_back(p0);
        mesh.positions.push_back(p1);
        mesh.positions.push_back(p2);
        mesh.positions.push_back(p3);
        
        for (int i = 0; i < 4; ++i) {
            mesh.normals.push_back(normal);
            mesh.colors.push_back(color);
        }
        
        // CCW Triangle Winding
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);
    };

    glm::vec3 white(0.75f, 0.75f, 0.75f);
    glm::vec3 red(0.75f, 0.12f, 0.12f);
    glm::vec3 green(0.12f, 0.75f, 0.12f);
    glm::vec3 light_color(15.0f, 15.0f, 15.0f);

    // Floor (y = 0) -> Normal points UP (+Y)
    add_quad(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(1, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), white);
    
    // Ceiling (y = 1) -> Normal points DOWN (-Y)
    add_quad(glm::vec3(0, 1, 0), glm::vec3(1, 1, 0), glm::vec3(1, 1, 1), glm::vec3(0, 1, 1), glm::vec3(0, -1, 0), white);
    
    // Back Wall (z = 0) -> Normal points FORWARD (+Z)
    add_quad(glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(1, 1, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), white);
    
    // Left Wall (x = 0) -> RED -> Normal points RIGHT (+X)
    add_quad(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 1, 1), glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), red);
    
    // Right Wall (x = 1) -> GREEN -> Normal points LEFT (-X)
    add_quad(glm::vec3(1, 0, 0), glm::vec3(1, 0, 1), glm::vec3(1, 1, 1), glm::vec3(1, 1, 0), glm::vec3(-1, 0, 0), green);

    // Ceiling Light Patch (y = 0.99) -> Normal points DOWN (-Y)
    float l_min = 0.05f, l_max = 0.95f;
    add_quad(glm::vec3(l_min, 0.99f, l_min), glm::vec3(l_max, 0.99f, l_min), 
             glm::vec3(l_max, 0.99f, l_max), glm::vec3(l_min, 0.99f, l_max), 
             glm::vec3(0, -1, 0), light_color);

    setup_buffers();
}

void CornellBox::setup_buffers() {
    if (vao == 0) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
    }

    glBindVertexArray(vao);

    // Interleaved layout: Position (3) + Normal (3) + Color (3) = 9 floats per vertex
    std::vector<float> vertex_data;
    vertex_data.reserve(mesh.positions.size() * 9);
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        vertex_data.push_back(mesh.positions[i].x);
        vertex_data.push_back(mesh.positions[i].y);
        vertex_data.push_back(mesh.positions[i].z);
        vertex_data.push_back(mesh.normals[i].x);
        vertex_data.push_back(mesh.normals[i].y);
        vertex_data.push_back(mesh.normals[i].z);
        vertex_data.push_back(mesh.colors[i].x);
        vertex_data.push_back(mesh.colors[i].y);
        vertex_data.push_back(mesh.colors[i].z);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_data.size() * sizeof(float), vertex_data.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(GLuint), mesh.indices.data(), GL_STATIC_DRAW);

    GLsizei stride = 9 * sizeof(float);

    // Location 0: Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    // Location 1: Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Location 2: Color
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void CornellBox::load_from_dae(const std::string& filepath) {
    // TODO: Use Assimp to load DAE
    // For now, just fall back to procedural
    std::cerr << "DAE loading not yet implemented, using procedural box\n";
    generate_procedural();
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
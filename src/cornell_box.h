// cornell_box.h
#pragma once
#include <glm/glm.hpp>

// Platform-specific OpenGL headers
#ifdef _WIN32
    #include <GL/gl.h>
#else
    #include <OpenGL/gl3.h>
#endif

#include <vector>
#include <string>

struct Mesh {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> colors;
    std::vector<GLuint> indices;
};

class CornellBox {
public:
    CornellBox();
    ~CornellBox();

    // Load scene (procedural or from DAE)
    void generate_procedural();
    void load_from_dae(const std::string& filepath);

    // Render the box
    void render() const;

    // Access geometry for photon mapping / simulation
    const Mesh& get_mesh() const { return mesh; }

private:
    Mesh mesh;
    GLuint vao, vbo, ebo;

    void setup_buffers();
    void cleanup();

    // Procedural generation helpers
    void add_box_face(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3);
};
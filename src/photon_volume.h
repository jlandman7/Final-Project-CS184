#pragma once
#include <glm/glm.hpp>
#include <vector>

struct Photon {
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 energy;
    int bounces;
};

struct PhotonVolume {
    // 3D grid of photon cells
    std::vector<std::vector<Photon>> grid;
    
    int grid_size_x, grid_size_y, grid_size_z;
    glm::vec3 min_bounds, max_bounds;
    
    PhotonVolume(int res_x, int res_y, int res_z, 
                 glm::vec3 min_b = glm::vec3(0), 
                 glm::vec3 max_b = glm::vec3(1));
    
    void clear();
    void record_photon(const Photon& p);
    std::vector<Photon> query_radius(glm::vec3 pos, float radius) const;
    
private:
    void get_cell_index(glm::vec3 pos, int& x, int& y, int& z) const;
    int linearize_index(int x, int y, int z) const;
};
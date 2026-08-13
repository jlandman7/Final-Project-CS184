#pragma once
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include "water_simulation.h"

class Raytracer; 

struct Photon {
    glm::vec3 position;
    glm::vec3 incident_dir;
    glm::vec3 power;
};

class PhotonMapper {
public:
    PhotonMapper(const WaterSimulation& sim);
    
    void generate_caustic_map(int photon_count, const Raytracer& raytracer);
    void generate_gi_map(int photon_count, const Raytracer& raytracer);
    
    glm::vec3 estimate_caustic_intensity(const glm::vec3& hit_pos, float search_radius) const;
    glm::vec3 estimate_gi_intensity(const glm::vec3& hit_pos, float search_radius) const;

    const std::vector<Photon>& get_photons() const { return photons; }

private:
    const WaterSimulation& water_sim;
    
    // Flattened Caustic Data
    std::vector<Photon> photons;
    float cell_size = 0.05f; 
    int caustic_res = 20;
    std::vector<std::vector<size_t>> flat_spatial_grid;
    void build_spatial_grid();

    // Flattened GI Data
    std::vector<Photon> gi_photons;
    float gi_cell_size = 0.05f; 
    int gi_res = 20;
    std::vector<std::vector<size_t>> gi_flat_spatial_grid;
    void build_gi_spatial_grid();

    // Helper to safely map 3D cell coordinates to a 1D array index
    inline int get_1d_index(int x, int y, int z, int res) const {
        x = std::max(0, std::min(x, res - 1));
        y = std::max(0, std::min(y, res - 1));
        z = std::max(0, std::min(z, res - 1));
        return x + (y * res) + (z * res * res);
    }
};
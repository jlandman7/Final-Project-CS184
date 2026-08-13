// photon_mapper.h
#pragma once
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "water_simulation.h"

// Forward declaration to prevent circular includes
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
    
    // Fast spatial lookup: finds photons within search_radius of hit_pos
    std::vector<Photon> find_near_photons(const glm::vec3& hit_pos, float search_radius) const;

    // Getters (includes both names so main.cpp works cleanly)
    const std::vector<Photon>& get_photons() const { return photons; }
    const std::vector<Photon>& get_caustic_map() const { return photons; }

private:
    const WaterSimulation& water_sim;
    std::vector<Photon> photons;

    // Spatial Hash Grid parameters
    float cell_size = 0.05f; 
    std::unordered_map<int64_t, std::vector<size_t>> spatial_grid;

    int64_t hash_cell(int x, int y, int z) const {
        return ((int64_t)x * 73856093) ^ ((int64_t)y * 19349663) ^ ((int64_t)z * 83492791);
    }
    
    void build_spatial_grid();
};
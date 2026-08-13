// photon_mapper.cpp
#include "photon_mapper.h"
#include "raytracer.h"
#include <omp.h>
#include <glm/gtc/random.hpp>
#include <algorithm>

PhotonMapper::PhotonMapper(const WaterSimulation& sim) : water_sim(sim) {}

void PhotonMapper::generate_caustic_map(int photon_count, const Raytracer& raytracer) {
    photons.clear();
    
    std::vector<std::vector<Photon>> thread_photons;

    #pragma omp parallel
    {
        #pragma omp single
        thread_photons.resize(omp_get_num_threads());

        int thread_id = omp_get_thread_num();
        float eta = 1.0f / 1.333f;

        #pragma omp for schedule(dynamic)
        for (int i = 0; i < photon_count; ++i) {
            // Distribute photon origins randomly across the entire ceiling
            glm::vec3 light_pos(
                glm::linearRand(0.001f, 0.999f), // X bounds
                0.98f,                           // Ceiling height
                glm::linearRand(0.001f, 0.999f)  // Z bounds
            );

            // Shoot them straight down (like midday sun) or with a tiny angular spread
            glm::vec3 dir(0.0f, -1.0f, 0.0f); 

            // Optional: Give it a slight angle so the webs sweep across the floor
            //glm::vec3 dir = glm::normalize(glm::vec3(0.1f, -1.0f, 0.05f));

            glm::vec3 water_hit_pos, water_hit_normal;
            if (!raytracer.intersect_water_dda(light_pos, dir, water_hit_pos, water_hit_normal)) continue;

            glm::vec3 N = water_hit_normal;
            if (glm::dot(dir, N) > 0.0f) N = -N;

            // Refract photon downwards using manual Snell's law
            float cos_i = -glm::dot(N, dir);
            float sin2_t = eta * eta * (1.0f - cos_i * cos_i);
            if (sin2_t > 1.0f) continue; // TIR

            float cos_t = std::sqrt(1.0f - sin2_t);
            glm::vec3 refracted_dir = eta * dir + (eta * cos_i - cos_t) * N;

            HitRecord rec;
            glm::vec3 inv_dir = 1.0f / refracted_dir;
            raytracer.intersect_bvh(water_hit_pos + refracted_dir * 0.001f, refracted_dir, inv_dir, 0, rec);

            if (rec.hit) {
                Photon p;
                p.position = rec.position;
                p.incident_dir = refracted_dir;
                p.power = glm::vec3(12.0f / float(photon_count));
                thread_photons[thread_id].push_back(p);
            }
        }
    }

    for (const auto& tp : thread_photons) {
        photons.insert(photons.end(), tp.begin(), tp.end());
    }

    build_spatial_grid();
}

void PhotonMapper::build_spatial_grid() {
    spatial_grid.clear();
    for (size_t i = 0; i < photons.size(); ++i) {
        int cx = static_cast<int>(std::floor(photons[i].position.x / cell_size));
        int cy = static_cast<int>(std::floor(photons[i].position.y / cell_size));
        int cz = static_cast<int>(std::floor(photons[i].position.z / cell_size));
        
        spatial_grid[hash_cell(cx, cy, cz)].push_back(i);
    }
}

std::vector<Photon> PhotonMapper::find_near_photons(const glm::vec3& hit_pos, float search_radius) const {
    std::vector<Photon> result;
    float radius_sq = search_radius * search_radius;

    // Determine cell range to inspect
    int min_x = static_cast<int>(std::floor((hit_pos.x - search_radius) / cell_size));
    int max_x = static_cast<int>(std::floor((hit_pos.x + search_radius) / cell_size));
    int min_y = static_cast<int>(std::floor((hit_pos.y - search_radius) / cell_size));
    int max_y = static_cast<int>(std::floor((hit_pos.y + search_radius) / cell_size));
    int min_z = static_cast<int>(std::floor((hit_pos.z - search_radius) / cell_size));
    int max_z = static_cast<int>(std::floor((hit_pos.z + search_radius) / cell_size));

    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int z = min_z; z <= max_z; ++z) {
                int64_t key = hash_cell(x, y, z);
                auto it = spatial_grid.find(key);
                if (it != spatial_grid.end()) {
                    for (size_t idx : it->second) {
                        const Photon& p = photons[idx];
                        glm::vec3 diff = p.position - hit_pos;
                        if (glm::dot(diff, diff) <= radius_sq) {
                            result.push_back(p);
                        }
                    }
                }
            }
        }
    }
    return result;
}
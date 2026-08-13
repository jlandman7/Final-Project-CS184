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

        #pragma omp for schedule(dynamic, 1024)
        for (int i = 0; i < photon_count; ++i) {
            glm::vec3 light_pos(
                glm::linearRand(0.05f, 0.95f),
                0.98f,
                glm::linearRand(0.05f, 0.95f)
            );
            glm::vec3 light_color = glm::vec3(0.4f, 0.90f, 1.0f);

            // Parallel downward rays create clean, sharp focal lines
            glm::vec3 dir = glm::vec3(0.0f, -1.0f, 0.0f);

            glm::vec3 water_hit_pos, water_hit_normal;
            if (!raytracer.intersect_water_dda(light_pos, dir, water_hit_pos, water_hit_normal)) continue;

            glm::vec3 N = water_hit_normal;
            if (glm::dot(dir, N) > 0.0f) N = -N;

            float cos_i = -glm::dot(N, dir);

            // 1. REFLECTION CAUSTICS (Upper Walls)
            glm::vec3 refl_dir = glm::reflect(dir, N);
            HitRecord refl_rec;
            glm::vec3 inv_refl_dir = 1.0f / refl_dir;
            raytracer.intersect_bvh(water_hit_pos + N * 0.002f, refl_dir, inv_refl_dir, 0, refl_rec);

            if (refl_rec.hit) {
                Photon p;
                p.position = refl_rec.position;
                p.incident_dir = refl_dir;
                p.power = glm::vec3(2.5f / float(photon_count));
                thread_photons[thread_id].push_back(p);
            }

            // 2. REFRACTION CAUSTICS (Floor & Submerged Walls)
            float sin2_t = eta * eta * (1.0f - cos_i * cos_i);
            if (sin2_t <= 1.0f) {
                float cos_t = std::sqrt(1.0f - sin2_t);
                glm::vec3 refracted_dir = eta * dir + (eta * cos_i - cos_t) * N;

                HitRecord refr_rec;
                glm::vec3 inv_refr_dir = 1.0f / refracted_dir;
                raytracer.intersect_bvh(water_hit_pos - N * 0.002f, refracted_dir, inv_refr_dir, 0, refr_rec);

                // Inside generate_caustic_map()
                if (refr_rec.hit) {
                    Photon p;
                    p.position = refr_rec.position;
                    p.incident_dir = refracted_dir;
                    p.power = glm::vec3(1.0f / float(photon_count)); // Reduced from 5.0f
                    thread_photons[thread_id].push_back(p);
                }
            }
        }
    }

    for (const auto& tp : thread_photons) {
        photons.insert(photons.end(), tp.begin(), tp.end());
    }

    build_spatial_grid();
}

void PhotonMapper::build_spatial_grid() {
    caustic_res = static_cast<int>(std::ceil(1.0f / cell_size));
    int total_cells = caustic_res * caustic_res * caustic_res;
    
    if (flat_spatial_grid.size() != total_cells) {
        flat_spatial_grid.assign(total_cells, std::vector<size_t>());
    } else {
        for (auto& cell : flat_spatial_grid) cell.clear();
    }

    for (size_t i = 0; i < photons.size(); ++i) {
        int cx = static_cast<int>(std::floor(photons[i].position.x / cell_size));
        int cy = static_cast<int>(std::floor(photons[i].position.y / cell_size));
        int cz = static_cast<int>(std::floor(photons[i].position.z / cell_size));
        
        int idx = get_1d_index(cx, cy, cz, caustic_res);
        flat_spatial_grid[idx].push_back(i);
    }
}

void PhotonMapper::build_gi_spatial_grid() {
    gi_res = static_cast<int>(std::ceil(1.0f / gi_cell_size));
    int total_cells = gi_res * gi_res * gi_res;
    
    if (gi_flat_spatial_grid.size() != total_cells) {
        gi_flat_spatial_grid.assign(total_cells, std::vector<size_t>());
    } else {
        for (auto& cell : gi_flat_spatial_grid) cell.clear();
    }

    for (size_t i = 0; i < gi_photons.size(); ++i) {
        int cx = static_cast<int>(std::floor(gi_photons[i].position.x / gi_cell_size));
        int cy = static_cast<int>(std::floor(gi_photons[i].position.y / gi_cell_size));
        int cz = static_cast<int>(std::floor(gi_photons[i].position.z / gi_cell_size));
        
        int idx = get_1d_index(cx, cy, cz, gi_res);
        gi_flat_spatial_grid[idx].push_back(i);
    }
}

glm::vec3 PhotonMapper::estimate_caustic_intensity(const glm::vec3& hit_pos, float search_radius) const {
    glm::vec3 accumulated_flux(0.0f);
    float radius_sq = search_radius * search_radius;
    float inv_radius_sq = 1.0f / radius_sq;

    int min_x = std::max(0, static_cast<int>(std::floor((hit_pos.x - search_radius) / cell_size)));
    int max_x = std::min(caustic_res - 1, static_cast<int>(std::floor((hit_pos.x + search_radius) / cell_size)));
    int min_y = std::max(0, static_cast<int>(std::floor((hit_pos.y - search_radius) / cell_size)));
    int max_y = std::min(caustic_res - 1, static_cast<int>(std::floor((hit_pos.y + search_radius) / cell_size)));
    int min_z = std::max(0, static_cast<int>(std::floor((hit_pos.z - search_radius) / cell_size)));
    int max_z = std::min(caustic_res - 1, static_cast<int>(std::floor((hit_pos.z + search_radius) / cell_size)));

    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int z = min_z; z <= max_z; ++z) {
                int idx = x + (y * caustic_res) + (z * caustic_res * caustic_res);
                for (size_t p_idx : flat_spatial_grid[idx]) {
                    const Photon& p = photons[p_idx];
                    glm::vec3 diff = p.position - hit_pos;
                    float dist_sq = glm::dot(diff, diff);
                    
                    if (dist_sq <= radius_sq) {
                        float weight = 1.0f - (dist_sq * inv_radius_sq);
                        accumulated_flux += p.power * weight;
                    }
                }
            }
        }
    }

    const float pi = 3.14159265f;
    // Correct area normalization for quadratic weight integral over 2D disk
    float area = 0.5f * pi * radius_sq; 
    return accumulated_flux / area;
}

glm::vec3 PhotonMapper::estimate_gi_intensity(const glm::vec3& hit_pos, float search_radius) const {
    glm::vec3 accumulated_flux(0.0f);
    float radius_sq = search_radius * search_radius;
    float inv_radius_sq = 1.0f / radius_sq;

    int min_x = std::max(0, static_cast<int>(std::floor((hit_pos.x - search_radius) / gi_cell_size)));
    int max_x = std::min(gi_res - 1, static_cast<int>(std::floor((hit_pos.x + search_radius) / gi_cell_size)));
    int min_y = std::max(0, static_cast<int>(std::floor((hit_pos.y - search_radius) / gi_cell_size)));
    int max_y = std::min(gi_res - 1, static_cast<int>(std::floor((hit_pos.y + search_radius) / gi_cell_size)));
    int min_z = std::max(0, static_cast<int>(std::floor((hit_pos.z - search_radius) / gi_cell_size)));
    int max_z = std::min(gi_res - 1, static_cast<int>(std::floor((hit_pos.z + search_radius) / gi_cell_size)));

    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int z = min_z; z <= max_z; ++z) {
                int idx = x + (y * gi_res) + (z * gi_res * gi_res);
                for (size_t p_idx : gi_flat_spatial_grid[idx]) {
                    const Photon& p = gi_photons[p_idx];
                    glm::vec3 diff = p.position - hit_pos;
                    float dist_sq = glm::dot(diff, diff);
                    
                    if (dist_sq <= radius_sq) {
                        float dist = std::sqrt(dist_sq);
                        float weight = 1.0f - (dist / search_radius);
                        accumulated_flux += p.power * weight;
                    }
                }
            }
        }
    }

    const float pi = 3.14159265f;
    float area = pi * search_radius * search_radius;
    return accumulated_flux / area;
}

void PhotonMapper::generate_gi_map(int photon_count, const Raytracer& raytracer) {
    gi_photons.clear();
    std::vector<std::vector<Photon>> thread_photons;

    #pragma omp parallel
    {
        #pragma omp single
        thread_photons.resize(omp_get_num_threads());
        int thread_id = omp_get_thread_num();

        #pragma omp for schedule(dynamic, 1024)
        for (int i = 0; i < photon_count; ++i) {
            // Uniform area light on the ceiling
            glm::vec3 light_pos(glm::linearRand(0.01f, 0.99f), 0.98f, glm::linearRand(0.01f, 0.99f));
            glm::vec3 dir(0.0f, -1.0f, 0.0f);

            HitRecord rec1;
            glm::vec3 inv_dir = 1.0f / dir;
            raytracer.intersect_bvh(light_pos, dir, inv_dir, 0, rec1);

            if (rec1.hit) {
                // Generate a cosine-weighted diffuse bounce direction
                glm::vec2 r = glm::linearRand(glm::vec2(0.0f), glm::vec2(1.0f));
                float phi = 2.0f * 3.14159265f * r.x;
                float cos_theta = std::sqrt(1.0f - r.y);
                float sin_theta = std::sqrt(r.y);
                
                glm::vec3 w = rec1.normal;
                glm::vec3 u = glm::normalize(glm::cross((std::abs(w.x) > 0.1f ? glm::vec3(0,1,0) : glm::vec3(1,0,0)), w));
                glm::vec3 v = glm::cross(w, u);
                glm::vec3 bounce_dir = glm::normalize(u * std::cos(phi) * sin_theta + v * std::sin(phi) * sin_theta + w * cos_theta);

                // Trace the bounced ray to its final resting place
                HitRecord rec2;
                glm::vec3 bounce_inv_dir = 1.0f / bounce_dir;
                raytracer.intersect_bvh(rec1.position + bounce_dir * 0.005f, bounce_dir, bounce_inv_dir, 0, rec2);

                if (rec2.hit) {
                    Photon p;
                    p.position = rec2.position;
                    p.incident_dir = bounce_dir;
                    // Color the photon with the albedo of the surface it bounced off of!
                    p.power = rec1.color * (6.0f / float(photon_count)); 
                    thread_photons[thread_id].push_back(p);
                }
            }
        }
    }

    for (const auto& tp : thread_photons) {
        gi_photons.insert(gi_photons.end(), tp.begin(), tp.end());
    }
    build_gi_spatial_grid();
}
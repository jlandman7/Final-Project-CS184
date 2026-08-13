// water_simulation.cpp
#include "water_simulation.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <iostream>

WaterSimulation::WaterSimulation(const WaterSimulationConfig& config)
    : config(config),
      resolution(config.grid_resolution),
      domain_size(config.domain_size),
      wave_speed(config.wave_speed),
      damping(config.damping),
      harmonic_count(config.harmonic_count),
      dt(config.simulation_timestep) {
    
    // Validate Nyquist limit
    if (harmonic_count > resolution / 2) {
        std::cout << "Warning: harmonic_count (" << harmonic_count 
                  << ") exceeds Nyquist limit (" << resolution / 2 
                  << "). Clamping.\n";
        harmonic_count = resolution / 2;
    }

    // Allocate height field buffers
    int grid_size = resolution * resolution;
    heights.resize(grid_size, 0.0f);
    heights_prev.resize(grid_size, 0.0f);
    heights_vel.resize(grid_size, 0.0f);

    std::cout << "WaterSimulation initialized:\n";
    std::cout << "  Grid: " << resolution << "x" << resolution << "\n";
    std::cout << "  Wave speed: " << wave_speed << "\n";
    std::cout << "  Damping: " << damping << "\n";
    std::cout << "  Harmonics: " << harmonic_count << "\n";
}

WaterSimulation::~WaterSimulation() {
}

void WaterSimulation::initialize() {
    switch (config.init_mode) {
        case WaterInitializationMode::Cosine:
            initialize_cosine_waves();
            break;
        case WaterInitializationMode::Directional:
            initialize_directional_waves();
            break;
        case WaterInitializationMode::Gaussian_Drops:
            initialize_gaussian_drops();
            break;
    }

    // Copy to previous frame for first step
    heights_prev = heights;

    std::cout << "Water surface initialized\n";
}

void WaterSimulation::initialize_cosine_waves() {
    // Base height (2/5 of box height = 0.4)
    float base_height = 0.4f;
    float perturbation = 0.1f;  // Small initial displacement

    float dx = domain_size / (resolution - 1);

    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            float px = x * dx;
            float py = y * dx;

            // Superpose cosine waves with different frequencies
            float height = base_height;
            for (int k = 1; k <= harmonic_count; ++k) {
                float freq = k * 2.0f * M_PI / domain_size;
                float amplitude = perturbation / k;  // Decay amplitude with frequency
                height += amplitude * std::cos(freq * px) * std::cos(freq * py);
            }

            heights[index(x, y)] = height;
        }
    }
}

void WaterSimulation::initialize_directional_waves() {
    float base_height = 0.4f;
    float dx = domain_size / (resolution - 1);

    std::srand(12345); 

    // Define a dominant wind direction and a max spread to keep waves cohesive
    float dominant_angle = M_PI / 4.0f; 
    float angle_spread = M_PI / 3.0f; 

    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            float px = x * dx;
            float py = y * dx;
            float height = base_height;

            for (int k = 1; k <= harmonic_count; ++k) {
                float phase = (std::rand() / (float)RAND_MAX) * 2.0f * M_PI;
                
                // Cluster the wave directions around the dominant angle
                float angle_offset = ((std::rand() / (float)RAND_MAX) * 2.0f - 1.0f) * angle_spread;
                float current_angle = dominant_angle + angle_offset;
                glm::vec2 direction(std::cos(current_angle), std::sin(current_angle));
                
                // Scale frequency slower, and decay amplitude exponentially (k^2)
                float freq = (1.0f + k * 0.5f) * 2.0f; 
                float amplitude = 0.06f / (pow(k, 20)); 
                
                float dot_val = (px * direction.x + py * direction.y);
                height += amplitude * std::cos(freq * dot_val + phase);
            }

            heights[index(x, y)] = height;
        }
    }
}

void WaterSimulation::initialize_gaussian_drops() {
    float base_height = 0.4f;
    float dx = domain_size / (resolution - 1);
    
    std::fill(heights.begin(), heights.end(), base_height);
    std::srand(54321); 

    // Generate a drop for every harmonic requested
    for (int k = 0; k < harmonic_count; ++k) {
        // Randomize center (kept slightly away from extreme edges)
        float cx = ((std::rand() / (float)RAND_MAX) * 0.8f + 0.1f) * domain_size;
        float cy = ((std::rand() / (float)RAND_MAX) * 0.8f + 0.1f) * domain_size;
        glm::vec2 drop_center(cx, cy);
        
        // Randomize radius between 2% and 15% of the domain size
        float drop_radius = ((std::rand() / (float)RAND_MAX) * 0.13f + 0.02f) * domain_size;
        
        // Randomize magnitude between 0.05 and 0.20
        float drop_magnitude = ((std::rand() / (float)RAND_MAX) * 0.15f + 0.05f);
        
        // 50/50 chance to be a peak or a trough
        if (std::rand() % 2 == 0) {
            drop_magnitude = -drop_magnitude;
        }

        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                float px = x * dx;
                float py = y * dx;
                
                float dist_sq = (px - drop_center.x) * (px - drop_center.x) + 
                                (py - drop_center.y) * (py - drop_center.y);
                
                if (dist_sq < (drop_radius * 3.0f) * (drop_radius * 3.0f)) {
                    float effect = std::exp(-dist_sq / (drop_radius * drop_radius));
                    heights[index(x, y)] += drop_magnitude * effect;
                }
            }
        }
    }

    // Clamp the final stacked result just in case drops overlap aggressively
    for (auto& h : heights) {
        h = std::clamp(h, 0.0f, 1.0f);
    }
}

void WaterSimulation::step() {
    float dx = domain_size / (resolution - 1);
    
    // Calculate the absolute maximum safe timestep based on the CFL condition
    // c * (dt / dx) <= 1 / sqrt(2) -> dt <= dx / (c * sqrt(2))
    // We multiply by 0.9f to leave a 10% safety margin
    float max_safe_dt = 0.9f * dx / (wave_speed * 1.41421356f);
    
    // Determine how many sub-steps we need to cover the full dt
    int sub_steps = std::ceil(dt / max_safe_dt);
    float actual_dt = dt / sub_steps;
    
    float c_sq_dt_sq = wave_speed * wave_speed * actual_dt * actual_dt;
    std::vector<float> heights_next(resolution * resolution);

    // Run the solver loop for however many sub-steps are required
    for (int s = 0; s < sub_steps; ++s) {
        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                int idx = index(x, y);

                float lap = laplacian(x, y);
                float h_curr = heights[idx];
                float h_prev = heights_prev[idx];

                float h_new = 2.0f * h_curr - h_prev + c_sq_dt_sq * lap;
                
                // Scale damping by actual_dt so it remains frame-rate independent
                h_new -= damping * actual_dt * (h_curr - h_prev);

                heights_next[idx] = std::clamp(h_new, 0.0f, 1.0f);
            }
        }

        // Shift time steps properly for the next sub-step
        heights_prev = heights;
        heights = heights_next; 
    }
}

float WaterSimulation::get_height(int x, int y) const {
    return heights[index(x, y)];
}

float WaterSimulation::laplacian(int x, int y) const {
    float dx = domain_size / (resolution - 1);
    float inv_dx_sq = 1.0f / (dx * dx);

    // Central differences: ∇²h = (h_left + h_right + h_top + h_bottom - 4*h) / dx²
    // With Neumann boundary conditions: mirror at edges

    auto get_height_safe = [this](int gx, int gy) -> float {
        // Clamp to grid, mirror at boundaries for Neumann BC
        if (gx < 0) gx = -gx;
        if (gx >= resolution) gx = 2 * resolution - 2 - gx;
        if (gy < 0) gy = -gy;
        if (gy >= resolution) gy = 2 * resolution - 2 - gy;

        return heights[index(gx, gy)];
    };

    float h_center = heights[index(x, y)];
    float h_left = get_height_safe(x - 1, y);
    float h_right = get_height_safe(x + 1, y);
    float h_top = get_height_safe(x, y - 1);
    float h_bottom = get_height_safe(x, y + 1);

    float lap = (h_left + h_right + h_top + h_bottom - 4.0f * h_center) * inv_dx_sq;
    return lap;
}
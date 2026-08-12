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
        case WaterInitializationMode::CosineWaves:
            initialize_cosine_waves();
            break;
        case WaterInitializationMode::FourierBessel:
            initialize_fourier_bessel();
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

void WaterSimulation::initialize_fourier_bessel() {
    // Fourier-Bessel: radially symmetric modes using Bessel J_0
    float base_height = 0.4f;
    float perturbation = 0.05f;

    float center_x = domain_size / 2.0f;
    float center_y = domain_size / 2.0f;
    float dx = domain_size / (resolution - 1);
    float max_radius = domain_size / 2.0f;

    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            float px = x * dx;
            float py = y * dx;
            float r = std::sqrt((px - center_x) * (px - center_x) + 
                               (py - center_y) * (py - center_y));

            float height = base_height;

            // Superpose Bessel modes
            // J_0(k*r) where k is chosen to vanish at boundary
            for (int m = 1; m <= harmonic_count; ++m) {
                // First m zeros of J_0 are approximately: 2.405, 5.520, 8.654, ...
                // Simplified: use k_m = m * pi / max_radius
                float k = m * M_PI / max_radius;
                float arg = k * r;

                // Approximate J_0 using series (for small arg)
                // Full implementation would use gsl_sf_bessel_J0
                // For now, use a simple approximation
                float bessel_val;
                if (arg < 8.0f) {
                    // Truncated series approximation of J_0
                    bessel_val = 1.0f - (arg*arg)/4.0f + (arg*arg*arg*arg)/64.0f;
                } else {
                    bessel_val = std::sqrt(2.0f / (M_PI * arg)) * std::cos(arg - M_PI/4.0f);
                }

                float amplitude = perturbation / m;
                height += amplitude * bessel_val;
            }

            heights[index(x, y)] = height;
        }
    }
}

void WaterSimulation::step() {
    float c_sq_dt_sq = wave_speed * wave_speed * dt * dt;
    std::vector<float> heights_next(resolution * resolution);

    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            int idx = index(x, y);

            float lap = laplacian(x, y);
            float h_curr = heights[idx];
            float h_prev = heights_prev[idx];

            float h_new = 2.0f * h_curr - h_prev + c_sq_dt_sq * lap;
            h_new -= damping * (h_curr - h_prev);

            heights_next[idx] = std::clamp(h_new, 0.0f, 1.0f);
        }
    }

    // Shift time steps properly
    heights_prev = heights;
    heights = std::move(heights_next);

    apply_boundary_conditions();
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

void WaterSimulation::apply_boundary_conditions() {
    // Neumann BC: ∂h/∂n = 0 at walls (no flux)
    // Implementation: mirror heights at boundaries
    
    for (int i = 0; i < resolution; ++i) {
        // Left and right edges (x = 0, x = resolution-1)
        heights[index(0, i)] = heights[index(1, i)];
        heights[index(resolution - 1, i)] = heights[index(resolution - 2, i)];
        
        // Top and bottom edges (y = 0, y = resolution-1)
        heights[index(i, 0)] = heights[index(i, 1)];
        heights[index(i, resolution - 1)] = heights[index(i, resolution - 2)];
    }
    
    // Corners (average of neighbors)
    heights[index(0, 0)] = (heights[index(1, 0)] + heights[index(0, 1)]) * 0.5f;
    heights[index(resolution - 1, 0)] = (heights[index(resolution - 2, 0)] + heights[index(resolution - 1, 1)]) * 0.5f;
    heights[index(0, resolution - 1)] = (heights[index(1, resolution - 1)] + heights[index(0, resolution - 2)]) * 0.5f;
    heights[index(resolution - 1, resolution - 1)] = (heights[index(resolution - 2, resolution - 1)] + heights[index(resolution - 1, resolution - 2)]) * 0.5f;
}

float WaterSimulation::get_height(int x, int y) const {
    if (x < 0 || x >= resolution || y < 0 || y >= resolution) {
        return 0.0f;
    }
    return heights[index(x, y)];
}
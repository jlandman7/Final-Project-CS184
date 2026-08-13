// water_simulation.h
#pragma once
#include <vector>
#include <glm/glm.hpp>

enum class WaterInitializationMode {
    Cosine,
    Directional,
    Gaussian_Drops
};

struct WaterSimulationConfig {
    int grid_resolution;           // N x N grid
    float domain_size;             // Physical size (0 to 1)
    float wave_speed;              // c in wave equation
    float damping;                 // Energy loss per step
    int harmonic_count;            // How many frequency components
    float simulation_timestep;     // dt
    WaterInitializationMode init_mode;
};

class WaterSimulation {
public:
    WaterSimulation(const WaterSimulationConfig& config);
    ~WaterSimulation();

    // Initialize water surface
    void initialize();

    // Step simulation forward by one timestep
    void step();

    // Get height at grid point
    float get_height(int x, int y) const;

    // Get entire height field for rendering
    const std::vector<float>& get_heights() const { return heights; }

    // Get grid info
    int get_resolution() const { return resolution; }
    float get_domain_size() const { return domain_size; }

private:
    WaterSimulationConfig config;
    int resolution;
    float domain_size;
    float wave_speed;
    float damping;
    int harmonic_count;
    float dt;

    std::vector<float> heights;      // Current height field
    std::vector<float> heights_prev; // Previous height field
    std::vector<float> heights_vel;  // Velocities (for Verlet integration)

    // Initialization helpers
    void initialize_cosine_waves();
    void initialize_directional_waves();
    void initialize_gaussian_drops();

    // Wave equation solver
    void update_heights();

    // Helpers
    int index(int x, int y) const { return y * resolution + x; }
    float laplacian(int x, int y) const;
    void apply_boundary_conditions();
};
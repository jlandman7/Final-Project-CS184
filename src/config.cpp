// config.cpp
#include "config.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Helper functions for parsing
namespace {
    std::string to_lower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    int parse_int(const std::string& arg, const std::string& flag_name) {
        try {
            return std::stoi(arg);
        } catch (...) {
            throw std::runtime_error("Invalid integer for " + flag_name + ": " + arg);
        }
    }

    float parse_float(const std::string& arg, const std::string& flag_name) {
        try {
            return std::stof(arg);
        } catch (...) {
            throw std::runtime_error("Invalid float for " + flag_name + ": " + arg);
        }
    }

    // Parse comma-separated output modes: "full,height,photons"
    int parse_output_modes(const std::string& arg) {
        int modes = 0;
        std::stringstream ss(arg);
        std::string mode_str;

        while (std::getline(ss, mode_str, ',')) {
            // Trim whitespace
            mode_str.erase(0, mode_str.find_first_not_of(" \t"));
            mode_str.erase(mode_str.find_last_not_of(" \t") + 1);
            
            std::string lower = to_lower(mode_str);
            if (lower == "full") modes |= static_cast<int>(OutputMode::Full);
            else if (lower == "height") modes |= static_cast<int>(OutputMode::Height);
            else if (lower == "photons") modes |= static_cast<int>(OutputMode::Photons);
            else throw std::runtime_error("Unknown output mode: " + mode_str);
        }

        if (modes == 0) {
            throw std::runtime_error("At least one output mode must be specified");
        }
        return modes;
    }

    WindowVisualization parse_window_mode(const std::string& arg) {
        std::string lower = to_lower(arg);
        if (lower == "none") return WindowVisualization::None;
        if (lower == "full") return WindowVisualization::Full;
        if (lower == "height") return WindowVisualization::Height;
        if (lower == "photons") return WindowVisualization::Photons;
        throw std::runtime_error("Unknown window mode: " + arg);
    }
}

// Preset defaults
AppConfig get_preset_defaults(PresetQuality preset) {
    AppConfig config;
    // Base defaults
    config.render.output_width = 1280;
    config.render.output_height = 720;
    config.render.output_fps = 30.0f;
    config.render.enable_gamma_correction = true;
    config.render.enable_tone_mapping = true;
    config.sim.timestep = 0.016f;
    config.sim.sim_steps_per_frame = 1;
    config.sim.heightfield_variance = 0.5f;
    config.sim.heightfield_resolution = 256;
    config.video.save_png_sequence = false;
    config.debug.window_mode = WindowVisualization::Full;
    config.debug.verbose_logging = false;
    config.frame_count = 300;

    if (preset == PresetQuality::Preview) {
        config.photons.photon_count = 10000;
        config.photons.photon_bounces = 3;
    } else {  // HighQuality
        config.photons.photon_count = 500000;
        config.photons.photon_bounces = 8;
    }

    return config;
}

AppConfig parse_cli(int argc, char* argv[]) {
    if (argc < 2) {
        throw std::runtime_error("Usage: water_sim [options]\n"
            "Required: --output <modes> (e.g., --output full,height,photons)\n"
            "Optional: --preset preview|hq (default: hq)\n"
            "          --input sim|pre-simulated (default: sim)\n"
            "          --cache-dir <path> (required if --input pre-simulated)\n"
            "          --dae <file> (default: cornell box)\n"
            "          --frames <n>\n"
            "          --photons <n>\n"
            "          --resolution <w>x<h>\n"
            "          --fps <f>\n"
            "          --output-dir <path> (required)\n"
            "          --output-name <name>\n"
            "          --window none|full|height|photons (default: full)\n"
            "          --verbose");
    }

    // Start with defaults
    AppConfig config = get_preset_defaults(PresetQuality::HighQuality);

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--preset" && i + 1 < argc) {
            std::string preset_str = to_lower(argv[++i]);
            if (preset_str == "preview") {
                config = get_preset_defaults(PresetQuality::Preview);
            } else if (preset_str == "hq") {
                config = get_preset_defaults(PresetQuality::HighQuality);
            } else {
                throw std::runtime_error("Unknown preset: " + preset_str);
            }
        }
        else if (arg == "--output" && i + 1 < argc) {
            config.output_modes = parse_output_modes(argv[++i]);
        }
        else if (arg == "--input" && i + 1 < argc) {
            std::string input_str = to_lower(argv[++i]);
            // input_mode_set = true;
            if (input_str == "sim") {
                config.input_mode = InputMode::Sim;
            } else if (input_str == "pre-simulated") {
                config.input_mode = InputMode::PreSimulated;
            } else {
                throw std::runtime_error("Unknown input mode: " + input_str);
            }
        }
        else if (arg == "--cache-dir" && i + 1 < argc) {
            config.pre_simulated_cache = argv[++i];
        }
        else if (arg == "--dae" && i + 1 < argc) {
            config.dae_file = argv[++i];
        }
        else if (arg == "--frames" && i + 1 < argc) {
            config.frame_count = parse_int(argv[++i], "--frames");
        }
        else if (arg == "--photons" && i + 1 < argc) {
            config.photons.photon_count = parse_int(argv[++i], "--photons");
        }
        else if (arg == "--resolution" && i + 1 < argc) {
            std::string res_str = argv[++i];
            size_t x_pos = res_str.find('x');
            if (x_pos == std::string::npos) {
                throw std::runtime_error("Resolution format: WIDTHxHEIGHT (e.g., 1280x720)");
            }
            config.render.output_width = parse_int(res_str.substr(0, x_pos), "--resolution width");
            config.render.output_height = parse_int(res_str.substr(x_pos + 1), "--resolution height");
        }
        else if (arg == "--fps" && i + 1 < argc) {
            config.render.output_fps = parse_float(argv[++i], "--fps");
        }
        else if (arg == "--output-dir" && i + 1 < argc) {
            config.video.output_directory = argv[++i];
        }
        else if (arg == "--output-name" && i + 1 < argc) {
            config.video.output_filename_base = argv[++i];
        }
        else if (arg == "--window" && i + 1 < argc) {
            config.debug.window_mode = parse_window_mode(argv[++i]);
        }
        else if (arg == "--verbose") {
            config.debug.verbose_logging = true;
        }
        else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    if (config.video.output_directory.empty()) {
            config.video.output_directory = "./output_dir";
        }

    // Validation
    if (config.video.output_directory.empty()) {
        throw std::runtime_error("--output-dir is required");
    }
    if (config.video.output_filename_base.empty()) {
        config.video.output_filename_base = "water_sim";
    }
    if (config.input_mode == InputMode::PreSimulated && config.pre_simulated_cache.empty()) {
        throw std::runtime_error("--cache-dir is required when using --input pre-simulated");
    }

    if (config.debug.verbose_logging) {
        config.print();
    }

    return config;
}

void AppConfig::print() const {
    std::cout << "\n=== Configuration ===\n";
    std::cout << "Input mode: " << (input_mode == InputMode::Sim ? "sim" : "pre-simulated") << "\n";
    std::cout << "Output modes: " << (output_modes & static_cast<int>(OutputMode::Full) ? "full " : "")
              << (output_modes & static_cast<int>(OutputMode::Height) ? "height " : "")
              << (output_modes & static_cast<int>(OutputMode::Photons) ? "photons " : "") << "\n";
    std::cout << "Frames: " << frame_count << "\n";
    std::cout << "Photon count: " << photons.photon_count << "\n";
    std::cout << "Resolution: " << render.output_width << "x" << render.output_height << "\n";
    std::cout << "Output FPS: " << render.output_fps << "\n";
    std::cout << "Output dir: " << video.output_directory << "\n";
    std::cout << "================\n\n";
}
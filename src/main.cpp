// main.cpp
#include "config.h"
#include "window.h"
#include "renderer.h"
#include "image_output.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <iomanip>

namespace fs = std::filesystem;

void write_metadata(const std::string& filepath, const AppConfig& config, double elapsed_seconds) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open metadata file: " + filepath);
    }

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    file << "=== Water Simulator Run Metadata ===\n";
    file << "Timestamp: " << std::ctime(&time);
    file << "\nConfiguration:\n";
    file << "  Input mode: " << (config.input_mode == InputMode::Sim ? "sim" : "pre-simulated") << "\n";
    file << "  Output modes: " << (config.output_modes & static_cast<int>(OutputMode::Full) ? "full " : "")
         << (config.output_modes & static_cast<int>(OutputMode::Height) ? "height " : "")
         << (config.output_modes & static_cast<int>(OutputMode::Photons) ? "photons " : "") << "\n";
    file << "  Frames: " << config.frame_count << "\n";
    file << "  Resolution: " << config.render.output_width << "x" << config.render.output_height << "\n";
    file << "  Output FPS: " << config.render.output_fps << "\n";
    file << "  Photon count: " << config.photons.photon_count << "\n";
    file << "\nRender Statistics:\n";
    file << "  Elapsed time: " << elapsed_seconds << "s\n";
    file << "  Time per frame: " << (elapsed_seconds / config.frame_count) << "s\n";
}

std::string encode_to_mp4(const AppConfig& config, const std::string& output_dir, const std::string& mode_suffix) {
    std::string png_pattern = (fs::path(output_dir) / "frames" / 
                               (config.video.output_filename_base + mode_suffix + "_%04d.png")).string();
    std::string output_mp4 = (fs::path(output_dir) / 
                              (config.video.output_filename_base + mode_suffix + ".mp4")).string();

    std::ostringstream cmd;
    cmd << "ffmpeg -y -framerate " << static_cast<int>(config.render.output_fps)
        << " -i \"" << png_pattern << "\" -c:v libx264 -pix_fmt yuv420p \"" << output_mp4 << "\" 2>/dev/null";

    std::cout << "Encoding: " << output_mp4 << "...\n";
    int ret = system(cmd.str().c_str());
    if (ret != 0) {
        std::cerr << "Warning: ffmpeg encoding failed (is ffmpeg installed?)\n";
    }
    return output_mp4;
}

int main(int argc, char* argv[]) {
    try {
        // Parse configuration
        AppConfig config = parse_cli(argc, argv);

        // Validate output directory exists
        if (!fs::exists(config.video.output_directory)) {
            throw std::runtime_error("Output directory does not exist: " + config.video.output_directory);
        }

        // Create timestamped subdirectory
        std::string run_dir = get_timestamped_output_dir(config.video.output_directory);
        fs::create_directories(run_dir);
        
        std::string frames_dir = fs::path(run_dir) / "frames";
        fs::create_directories(frames_dir);

        std::cout << "Output: " << run_dir << "\n\n";

        // Create window and renderer
        Window window(config.render.output_width, config.render.output_height, "Water Simulator");
        Renderer renderer(config.render.output_width, config.render.output_height);
        renderer.init();

        std::vector<glm::vec4> hdr_buffer;
        std::vector<uint8_t> ldr_frame;

        std::cout << "Starting render loop...\n";
        auto start_time = std::chrono::high_resolution_clock::now();

        for (int frame = 0; frame < config.frame_count; ++frame) {
            try {
                if (window.should_close()) break;

                float time = frame / config.render.output_fps;
                renderer.render_frame(time);

                // Read BEFORE swapping
                renderer.read_color_to_cpu(hdr_buffer);
                ldr_frame = ImageOutput::tone_map_to_ldr(hdr_buffer, 
                                                        config.render.output_width,
                                                        config.render.output_height);

                // Write PNG
                if (config.output_modes & static_cast<int>(OutputMode::Full)) {
                    std::ostringstream png_path;
                    png_path << frames_dir << "/" << config.video.output_filename_base << "_full_" 
                            << std::setfill('0') << std::setw(4) << frame << ".png";
                    ImageOutput::write_png(png_path.str(), ldr_frame, 
                                        config.render.output_width, config.render.output_height);
                }

                // Progress
                float progress = (frame + 1) / float(config.frame_count);
                int bar_width = 50;
                int filled = static_cast<int>(progress * bar_width);
                std::cout << "\r[";
                for (int i = 0; i < bar_width; ++i) {
                    std::cout << (i < filled ? "=" : " ");
                }
                std::cout << "] " << static_cast<int>(progress * 100.0f) << "% (" 
                        << (frame + 1) << "/" << config.frame_count << ")";
                std::cout.flush();

                window.poll_events();
            } catch (const std::exception& e) {
                std::cerr << "\nError on frame " << frame << ": " << e.what() << "\n";
                break;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        double elapsed = duration.count() / 1000.0;

        std::cout << "\n\nRender complete.\n";

        // Write metadata
        std::string metadata_file = fs::path(run_dir) / "metadata.txt";
        write_metadata(metadata_file, config, elapsed);
        std::cout << "Metadata: " << metadata_file << "\n";

        // Encode MP4
        if (config.output_modes & static_cast<int>(OutputMode::Full)) {
            encode_to_mp4(config, run_dir, "_full");
        }

        std::cout << "Total time: " << elapsed << "s\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(water_simulator CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Source files
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/window.cpp
    src/renderer.cpp
    src/cornell_box.cpp
    src/framebuffer.cpp
    src/image_output.cpp
)

set(HEADERS
    src/config.h
    src/window.h
    src/renderer.h
    src/cornell_box.h
    src/framebuffer.h
    src/image_output.h
)

# Create executable
add_executable(water_sim ${SOURCES} ${HEADERS})

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# Link libraries
target_link_libraries(water_sim
    PRIVATE
        OpenGL::GL
        glfw
        glm::glm
)

# Include directories
target_include_directories(water_sim
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Compiler flags
if(MSVC)
    target_compile_options(water_sim PRIVATE /W4 /O2)
else()
    target_compile_options(water_sim PRIVATE -Wall -Wextra -O2)

    # Silence OpenGL deprecation warnings on macOS
    if(APPLE)
        target_compile_definitions(water_sim PRIVATE GL_SILENCE_DEPRECATION)
    endif()
endif()

# Optional: add verbose output during configuration
message(STATUS "OpenGL: ${OPENGL_FOUND}")
message(STATUS "GLFW: ${glfw3_FOUND}")
message(STATUS "GLM: ${glm_FOUND}")
message(STATUS "Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "Executable will be in: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
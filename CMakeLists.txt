cmake_minimum_required(VERSION 3.16)
project(water_simulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(SOURCES
    src/main.cpp
    src/config.cpp
    src/window.cpp
    src/renderer.cpp
    src/cornell_box.cpp
    src/framebuffer.cpp
    src/image_output.cpp
    src/water_simulation.cpp
    src/water_mesh.cpp
)

set(HEADERS
    src/config.h
    src/window.h
    src/renderer.h
    src/cornell_box.h
    src/framebuffer.h
    src/image_output.h
    src/water_simulation.h
    src/water_mesh.h
)

add_executable(water_sim ${SOURCES} ${HEADERS})

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# Platform-specific linking
if(WIN32)
    target_link_libraries(water_sim
        PRIVATE
            opengl32
            glfw
            glm::glm
    )
else()
    target_link_libraries(water_sim
        PRIVATE
            OpenGL::GL
            glfw
            glm::glm
    )
endif()

target_include_directories(water_sim
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Compiler flags
if(MSVC)
    target_compile_options(water_sim PRIVATE /W4 /O2)
else()
    target_compile_options(water_sim PRIVATE -Wall -Wextra -O2)
    if(APPLE)
        target_compile_definitions(water_sim PRIVATE GL_SILENCE_DEPRECATION)
    endif()
endif()

message(STATUS "OpenGL: ${OPENGL_FOUND}")
message(STATUS "GLFW: ${glfw3_FOUND}")
message(STATUS "GLM: ${glm_FOUND}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Build directory: ${CMAKE_BINARY_DIR}")
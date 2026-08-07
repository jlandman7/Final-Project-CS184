// window.h
#pragma once
#include <GLFW/glfw3.h>
#include <string>

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    bool should_close() const;
    void close();
    void swap_buffers();
    void poll_events();

    int get_width() const { return width; }
    int get_height() const { return height; }

private:
    GLFWwindow* window;
    int width, height;
};
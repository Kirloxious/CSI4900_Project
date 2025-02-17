#pragma once


#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

class Window
{
private:
    GLFWwindow* m_Window;
public:
    int m_Width;
    int m_Height;
    const char* m_Title;

    Window(int width, int height, const char* title);
    ~Window();
    bool shouldClose();
    void makeCurrentContext();
    void swapBuffers();
    void pollEvents();
};


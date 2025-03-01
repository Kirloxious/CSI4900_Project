#pragma once


#include <GLFW/glfw3.h>
#include <iostream>

class Window
{
private:
public:
    GLFWwindow* m_Window;
    int m_Width;
    int m_Height;
    const char* m_Title;

    Window(int width, int height, const char* title);
    ~Window();
    bool shouldClose();
    void makeCurrentContext();
    void swapBuffers();
    void pollEvents();
    void getFrameBufferSize();
};


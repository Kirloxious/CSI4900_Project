#include "window.h"

Window::Window(int width, int height, const char* title){
    glewExperimental = GL_TRUE;
    glfwInit(); 
    glViewport(0, 0, width, height);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    m_Title = title;
    m_Height = height;
    m_Width = width;
}

Window::~Window(){
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

bool Window::shouldClose(){
    return glfwWindowShouldClose(m_Window);
}

void Window::makeCurrentContext(){
    glfwMakeContextCurrent(m_Window);
}

void Window::swapBuffers(){
    glfwSwapBuffers(m_Window);
}

void Window::pollEvents(){
    glfwPollEvents();
}
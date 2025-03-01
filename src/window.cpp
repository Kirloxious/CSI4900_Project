#include "window.h"

Window::Window(int width, int height, const char* title){
    glfwInit(); 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
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

void Window::getFrameBufferSize(){
    glfwGetFramebufferSize(m_Window, &m_Width, &m_Height);
}
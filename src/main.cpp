#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <chrono>

#include "renderer.h"
#include "window.h"
#include "compute_shader.h"
#include "shader.h"

#define MAX_NUM_SPHERES 10

static ComputeShader compute;
static const std::filesystem::path computeShaderPath = "shader/compute_shader.glsl";

static void ErrorCallback(int error, const char* description)
{
	std::cerr << "Error: " << description << std::endl;
}

static double clockToMilliseconds(clock_t ticks){
    // units/(units/time) => time (seconds) * 1000 = milliseconds
    return (ticks/(double)CLOCKS_PER_SEC)*1000.0;
}

int main() {

    glfwSetErrorCallback(ErrorCallback);
    
    int width = 800;
    int height = 600;
    
    Window window(width, height, "window");
    
    window.makeCurrentContext();

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glfwSwapInterval(1);



    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;

    
    // World scene
    // Spheres format is [center_x, center_y, center_z, radius]
    glm::vec4 sphereData[MAX_NUM_SPHERES] = {
        glm::vec4(0.0f, 0.0f, -1.0f, 0.5f),
        glm::vec4(0.0f, -100.5f, -1.0f, 100.0f),
    };
    
    // Create and bind UBO for spheres
    GLuint ubo;
    glCreateBuffers(1, &ubo);;
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, MAX_NUM_SPHERES * sizeof(glm::vec4), sphereData, GL_STATIC_READ); //data upload
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo); // binding location
    
    
    
    unsigned int num_objects = sizeof(sphereData) / sizeof(glm::vec4);
    compute = ComputeShader(computeShaderPath);
    compute.use();
    compute.setInt("num_objects", num_objects);

    

    Texture texture = createTexture(window.m_Width, window.m_Height);

    FrameBuffer fb = createFrameBuffer(texture);

    GLuint queryID;
    glGenQueries(1, &queryID);

    int frameIndex = 0;
    int frameCount = 0;
    double deltaTime = 0;
    double lastTime = glfwGetTime();
    double timer = lastTime;

    while(!window.shouldClose()){
        
        
        // Compute 
        {
            compute.use();
            compute.setInt("time", clock());
            compute.setInt("frameIndex", frameIndex);
            
            // Double buffer texture    
            glBindImageTexture(0, texture.handle, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
            glBindImageTexture(1, texture.handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
            
            const GLuint workGroupSizeX = 16;
			const GLuint workGroupSizeY = 16;
            
			GLuint numGroupsX = (width + workGroupSizeX - 1) / workGroupSizeX;
			GLuint numGroupsY = (height + workGroupSizeY - 1) / workGroupSizeY;
            
            glBeginQuery(GL_TIME_ELAPSED, queryID); // Computer shader timer start
			glDispatchCompute(numGroupsX, numGroupsY, 1);
            glEndQuery(GL_TIME_ELAPSED);            // Computer shader timer end
            
            // make sure writing to image has finished before read
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            
        }

        GLuint64 executionTime;
        glGetQueryObjectui64v(queryID, GL_QUERY_RESULT, &executionTime); // computer shader timer result
        
        blitFrameBuffer(fb);
        
        window.swapBuffers();
        window.pollEvents();
        
        double currentTime = glfwGetTime();
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        ++frameIndex;
        ++frameCount;

        
        if (currentTime - timer >= 1.0) {
            std::cout << "FPS: " << frameCount << " | Frame Time: " << (1000.0 / frameCount) << " ms" << " | Compute Shader Time: " << (executionTime / 1e6) << " ms" << std::endl;
            frameCount = 0;
            timer = currentTime;
        }
    }

    return 0;
}

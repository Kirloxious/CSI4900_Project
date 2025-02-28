#include <glm/glm.hpp>
#include <iostream>
#include <vector>

#include "window.h"
#include "compute_shader.h"
#include "shader.h"

#define MAX_NUM_SPHERES 10


int main() {

    Window window(800, 600, "window");

    window.makeCurrentContext();

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    ComputeShader compute("shader/compute_shader.comp");
    Shader quadShader("shader/quad_vertex.vs", "shader/quad_frag.fs");


    quadShader.use();
    quadShader.setInt("tex", 0);

    // Setup texture

    const unsigned int TEXTURE_WIDTH = window.m_Width, TEXTURE_HEIGHT = window.m_Height;
    unsigned int texture;

    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL); 
   

    // Setup screen plane 

    unsigned int quadVAO;
    unsigned int quadVBO;

    float quadVertices[] = {
        // positions        // texture Coords
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };


    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

    //positions attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    //texture coords attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(quadVAO);
    glBindVertexArray(0);


    // World scene

    // Create and bind UBO for spheres
    GLuint ubo;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, MAX_NUM_SPHERES * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);


    // Spheres format is [center_x, center_y, center_z, radius]
    glm::vec4 sphereData[MAX_NUM_SPHERES] = {
        glm::vec4(0.0f, 0.0f, -1.0f, 0.5f),
        glm::vec4(0.0f, -100.5f, -1.0f, 100.0f),
    };

    unsigned int num_objects = sizeof(sphereData) / sizeof(glm::vec4);
    compute.use();
    compute.setInt("num_objects", num_objects);

    // Upload data
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(sphereData), sphereData);
    
    int frameIndex = 0;

    while(!window.shouldClose()){
        compute.use();
        compute.setInt("time", clock());
        compute.setInt("frameIndex", frameIndex);

        // Double buffer texture    
        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
        glBindImageTexture(1, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
 
        glDispatchCompute((unsigned int)TEXTURE_WIDTH/16, (unsigned int)TEXTURE_HEIGHT/16, 1);
        
        // make sure writing to image has finished before read
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // draw process
        glClear(GL_COLOR_BUFFER_BIT);
        quadShader.use();
        glBindVertexArray(quadVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        ++frameIndex;
        

        window.pollEvents();
        window.swapBuffers();
    }

    return 0;
}

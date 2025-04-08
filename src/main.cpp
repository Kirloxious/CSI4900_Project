#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <chrono>

#include "renderer.h"
#include "window.h"
#include "compute_shader.h"
#include "shader.h"
#include "world.h"
#include "camera.h"

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
    
    CameraSettings camSettings{};
    camSettings.aspect_ratio = 16.0f / 9.0f;
    camSettings.image_width = 1080;
    camSettings.samples_per_pixel = 10;
    camSettings.max_depth = 10;
    camSettings.vfov = 20.0;
    camSettings.focus_dist = 20.0;
    camSettings.defocus_angle = 0.5;
    camSettings.lookfrom = glm::vec3(-2, 2, 1);
    camSettings.lookat = glm::vec3(0, 0, -1);
    camSettings.vup = glm::vec3(0, 1, 0);

    Camera camera = Camera(camSettings);
    
    Window window(camera.image_width, camera.image_height, "window");
    
    window.makeCurrentContext();

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glfwSwapInterval(0); // disable vsync



    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;

    
    // World scene
    // Spheres format is [center_x, center_y, center_z, radius, material_index]
    Sphere spheres[MAX_NUM_SPHERES] = {
        Sphere{glm::vec3( 0.0f,    0.0f, -1.2f), 0.5f, 0},  // mid sphere
        Sphere{glm::vec3(-1.0,    0.0, -1.0), 0.5f, 3},     // left sphere
        Sphere{glm::vec3( 1.0,    0.0, -1.0), 0.5f, 2},     // right sphere
        Sphere{glm::vec3 (0.0f, -100.5f, -1.0f), 100.0f, 1}, // ground
    };

    // Materials format is [r, g, b, a] where a is the fuzziness level or the refraction index
    Material materials[MAX_NUM_SPHERES] = {
        Material{glm::vec4(0.0f, 1.0f, 0.0f, 0.0f), 0.0f}, // mid mat
        Material{glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), 0.0f}, // ground mat
        Material{glm::vec4(0.8, 0.8, 0.8, 0.3), 0.0f}, // right mat
        Material{glm::vec4(1.0, 1.0, 1.0, -1.0), 1.0 / 1.5f}, // left mat
    };
    
    // Create and bind UBO for spheres
    GLuint spheres_ubo;
    glCreateBuffers(1, &spheres_ubo);;
    glBindBuffer(GL_UNIFORM_BUFFER, spheres_ubo);
    glBufferData(GL_UNIFORM_BUFFER, MAX_NUM_SPHERES * sizeof(Sphere), spheres, GL_STATIC_READ); //data upload
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, spheres_ubo); // binding location

    // Create and bind UBO for materials
    GLuint mats_ubo;
    glCreateBuffers(1, &mats_ubo);;
    glBindBuffer(GL_UNIFORM_BUFFER, mats_ubo);
    glBufferData(GL_UNIFORM_BUFFER, MAX_NUM_SPHERES * sizeof(Material), materials, GL_STATIC_READ); //data upload
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, mats_ubo); // binding location
    

    // Create and bind UBO for camera
    GLuint cam_ubo;
    glCreateBuffers(1, &cam_ubo);;
    glBindBuffer(GL_UNIFORM_BUFFER, cam_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraData), &camera.data, GL_STATIC_READ); //data upload
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, cam_ubo); // binding location


    unsigned int num_objects = sizeof(spheres) / sizeof(Sphere);
    std::cout << sizeof(spheres) / sizeof(Sphere) << std::endl;
    compute = ComputeShader(computeShaderPath);
    compute.use();
    compute.setInt("num_objects", num_objects);
    compute.setVec2("imageDimensions", glm::vec2(camera.image_width, camera.image_height));
    

    Texture texture = createTexture(window.m_Width, window.m_Height);

    FrameBuffer fb = createFrameBuffer(texture);

    GLuint queryID;
    glGenQueries(1, &queryID);

    int frameIndex = 0; // used for accumulating image
    int frameCount = 0; // fps counting
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
            
			GLuint numGroupsX = (camera.image_width + workGroupSizeX - 1) / workGroupSizeX;
			GLuint numGroupsY = (camera.image_height + workGroupSizeY - 1) / workGroupSizeY;
            
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

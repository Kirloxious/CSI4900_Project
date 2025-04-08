#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <random>

#include "renderer.h"
#include "window.h"
#include "compute_shader.h"
#include "shader.h"
#include "world.h"
#include "camera.h"
#include "bvh.h"

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

float randomFloat() {
    static std::mt19937 generator(std::random_device{}()); // internal state but no manual seed
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(generator);
}

int main() {


    glfwSetErrorCallback(ErrorCallback);
    
    CameraSettings camSettings{};
    camSettings.aspect_ratio = 16.0f / 9.0f;
    camSettings.image_width = 1200;
    
    camSettings.samples_per_pixel = 10;
    camSettings.max_depth = 10;
    
    camSettings.vfov = 20.0;
    camSettings.focus_dist = 10.0;
    camSettings.defocus_angle = 0.0;

    camSettings.lookfrom = glm::vec3(13, 2, 3);
    camSettings.lookat = glm::vec3(0, 0, 0);
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
    // std::vector<Sphere> spheres = {
    //     Sphere{glm::vec3 (0.0f, -100.5f, -1.0f), 100.0f, 1}, // ground
    //     Sphere{glm::vec3( 0.0f,    0.0f, -1.2f), 0.5f, 0},  // mid sphere
    //     Sphere{glm::vec3(-1.0,    0.0, -1.0), 0.5f, 3},     // left sphere
    //     Sphere{glm::vec3( 1.0,    0.0, -1.0), 0.5f, 2},     // right sphere
    // };

    // // Materials format is [r, g, b, a] where a is the fuzziness level or the refraction index
    // std::vector<Material> materials = {
    //     Material{glm::vec4(0.0f, 1.0f, 0.0f, 0.0f), 0.0f}, // mid mat
    //     Material{glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), 0.0f}, // ground mat
    //     Material{glm::vec4(0.8, 0.8, 0.8, 0.3), 0.0f}, // right mat
    //     Material{glm::vec4(1.0, 1.0, 1.0, -1.0), 1.0 / 1.5f}, // left mat
    // };
    
    std::vector<Sphere> spheres;
    // spheres.reserve(25);
    std::vector<Material> materials;
    // materials.reserve(25);

    // Ground sphere and mat
    materials.push_back(Lambertian(glm::vec3(0.5f, 0.5f, 0.5f)));
    spheres.push_back(createSphere(glm::vec3(0.0f, -1000.0f, 0.0f), 1000.0f, materials.size() - 1));

    std::cout << randomFloat() << std::endl;

    for(int a = -11; a < 11; a++) {
        for(int b = -11; b < 11; b++) {
            float choose_mat = randomFloat();
            glm::vec3 center = glm::vec3(a + 0.9f * randomFloat(), 0.2f, b + 0.9f * randomFloat());
            if (choose_mat < 0.8f) {
                // diffuse
                glm::vec3 color = glm::vec3(randomFloat(), randomFloat(), randomFloat());
                materials.push_back(Lambertian(color));
                spheres.push_back(createSphere(center, 0.2f, materials.size() - 1));

            }
            else if (choose_mat < 0.95f) {
                // metal
                glm::vec3 color = glm::vec3(randomFloat(), randomFloat(), randomFloat());
                float fuzz = 0.5f * randomFloat();
                materials.push_back(Metal(color, fuzz));
                spheres.push_back(createSphere(center, 0.2f, materials.size() - 1));

            }
            else {
                // glass
                materials.push_back(Dielectric(1.5f));
                spheres.push_back(createSphere(center, 0.2f, materials.size() - 1));
            }
        }
    }
    
    // big spheres
    materials.push_back(Dielectric(1.5f));
    spheres.push_back(createSphere(glm::vec3(0.0f, 1.0f, 0.0f), 1.0f, materials.size() - 1));
    
    materials.push_back(Lambertian(glm::vec3(0.4f, 0.2f, 0.1f)));
    spheres.push_back(createSphere(glm::vec3(-4.0f, 1.0f, 0.0f), 1.0f, materials.size() - 1));
    
    materials.push_back(Metal(glm::vec3(0.7f, 0.6f, 0.5f), 0.0f));
    spheres.push_back(createSphere(glm::vec3(4.0f, 1.0f, 0.0f), 1.0f, materials.size() - 1));
    
    std::vector<AABB> spheresAABBS;
    for (const auto& sphere : spheres) {
        spheresAABBS.push_back(computeAABB(sphere));
    }
    std::cout << "Number of spheres: " << spheres.size() << std::endl;
    std::cout << spheresAABBS.size() << std::endl;

    std::vector<BVHNode> bvhNodes;
    std::vector<int> sphereIndices(spheres.size());
    std::iota(sphereIndices.begin(), sphereIndices.end(), 0); // [0, 1, 2, ..., N]
    
    int root = buildBVH(bvhNodes, spheres, spheresAABBS, sphereIndices);

    std::vector<BVHNodeFlat> bvhFlat;
    bvhFlat.reserve(bvhNodes.size());

    for (const auto& node : bvhNodes) {
        BVHNodeFlat flat;
        flat.aabbMin = glm::vec4(node.aabb.min, 0.0f);
        flat.aabbMax = glm::vec4(node.aabb.max, 0.0f);
        flat.meta = glm::ivec4(node.left, node.right, node.sphereIndex, 0);
        bvhFlat.push_back(flat);
    }

    std::cout << "Number of BVH nodes: " << bvhNodes.size() << std::endl;
    std::cout << "Root index: " << root << std::endl;


    // Create and bind SSBO for spheres
    GLuint spheres_ssbo;
    glCreateBuffers(1, &spheres_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, spheres_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, spheres.size() * sizeof(Sphere), spheres.data(), GL_DYNAMIC_READ); //data upload
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, spheres_ssbo); // binding location
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Create and bind SSBO for materials
    GLuint mats_ssbo;
    glCreateBuffers(1, &mats_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mats_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, materials.size() * sizeof(Material), materials.data(), GL_DYNAMIC_READ); //data upload
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mats_ssbo); // binding location
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    

    // Create and bind UBO for camera
    GLuint cam_ubo;
    glCreateBuffers(1, &cam_ubo);;
    glBindBuffer(GL_UNIFORM_BUFFER, cam_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraData), &camera.data, GL_STATIC_READ); //data upload
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, cam_ubo); // binding location
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Create and bind SSBO for bvh nodes
    GLuint bvhnodes_ssbo;
    glCreateBuffers(1, &bvhnodes_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhnodes_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bvhFlat.size() * sizeof(BVHNodeFlat), bvhFlat.data(), GL_DYNAMIC_READ); //data upload
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, bvhnodes_ssbo); // binding location
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    unsigned int num_objects = spheres.size() * sizeof(Sphere);
    compute = ComputeShader(computeShaderPath);
    compute.use();
    compute.setInt("num_objects", num_objects);
    compute.setVec2("imageDimensions", glm::vec2(camera.image_width, camera.image_height));
    compute.setInt("bvh_size", bvhNodes.size());
    compute.setInt("root_index", root);
    

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
            std::cout << "FPS: " << frameCount << " | Frame Time: " << (1000.0 / float(frameCount)) << " ms" << " | Compute Shader Time: " << (executionTime / 1e6) << " ms" << std::endl;
            frameCount = 0;
            timer = currentTime;
        }
    }


    return 0;
}

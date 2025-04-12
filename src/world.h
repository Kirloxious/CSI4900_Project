#pragma once

#include <glm/glm.hpp>

struct alignas(16) Sphere
{
    glm::vec3 position;
    float radius;
    uint32_t material_index;
};

Sphere createSphere(glm::vec3 position, float radius, uint32_t material_index){
    Sphere sphere;
    sphere.position = position;
    sphere.radius = radius;
    sphere.material_index = material_index;
    return sphere;
}

const float MAT_LAMBERTIAN = 0.0;
const float MAT_METAL = 1.0;
const float MAT_DIELECTRIC = 2.0;
const float MAT_EMISSIVE = 3.0;

struct alignas(16) Material
{
    glm::vec3 color;
    float fuzz;
    glm::vec3 emission;
    float refractive_index;
    float type;
};

Material Dielectric(float refractive_index)
{
    Material material;
    material.color = glm::vec4(1.0f, 1.0f, 1.0f, -2.0f);
    material.fuzz = 0.0f;
    material.emission = glm::vec3(0.0f);
    material.refractive_index = refractive_index;
    material.type = MAT_DIELECTRIC;
    return material;
}

Material Lambertian(glm::vec3 color)
{
    Material material;
    material.color = color;
    material.fuzz = 0.0f;
    material.emission = glm::vec3(0.0f);
    material.refractive_index = 0.0f;
    material.type = MAT_LAMBERTIAN;
    return material;
}

Material Metal(glm::vec3 color, float fuzz)
{
    Material material;
    material.color = color;
    material.fuzz = fuzz;
    material.emission = glm::vec3(0.0f);
    material.refractive_index = 0.0f;
    material.type = MAT_METAL;
    return material;
}

Material Emissive(glm::vec3 color, glm::vec3 emission)
{
    Material material;
    material.color = color;
    material.fuzz = 0.0f;
    material.emission = emission;
    material.refractive_index = 0.0f;
    material.type = MAT_EMISSIVE;
    return material;
}

struct World
{
    /* data */
};



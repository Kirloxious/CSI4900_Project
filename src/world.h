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
struct alignas(16) Material
{
    glm::vec4 color;
    float refractive_index;
};

Material Dielectric(float refractive_index)
{
    Material material;
    material.color = glm::vec4(1.0f, 1.0f, 1.0f, -2.0f);
    material.refractive_index = refractive_index;
    return material;
}

Material Lambertian(glm::vec3 color)
{
    Material material;
    material.color = glm::vec4(color, -1.0f);
    material.refractive_index = 0.0f;
    return material;
}

Material Metal(glm::vec3 color, float fuzz)
{
    Material material;
    material.color = glm::vec4(color, fuzz);
    material.refractive_index = 0.0f;
    return material;
}


struct World
{
    /* data */
};



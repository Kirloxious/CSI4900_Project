#pragma once

#include <glm/glm.hpp>

struct alignas(16) Sphere
{
    glm::vec3 position;
    float radius;
    uint32_t material_index;
};

struct alignas(16) Material
{
    glm::vec4 color;
};


struct World
{
    /* data */
};

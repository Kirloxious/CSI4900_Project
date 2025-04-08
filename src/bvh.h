#pragma once

#include "world.h"
#include <vector>
#include <iostream>
#include <algorithm>

struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    glm::vec3 center() const {
        return (min + max) * 0.5f;
    }
};

AABB computeAABB(const Sphere& s) {
    glm::vec3 rvec(s.radius);
    return { s.position - rvec, s.position + rvec };
}

AABB surroundingBox(const AABB& a, const AABB& b) {
    return {
        glm::min(a.min, b.min),
        glm::max(a.max, b.max)
    };
}

struct BVHNode {
    AABB aabb;
    int left;
    int right;
    int sphereIndex = -1;
};

struct alignas(16) BVHNodeFlat {
    glm::vec4 aabbMin;   // .xyz = min
    glm::vec4 aabbMax;   // .xyz = max
    glm::ivec4 meta;     // .x = left, .y = right, .z = sphereIndex, .w = unused
};

int buildBVH(std::vector<BVHNode>& bvh, const std::vector<Sphere>& spheres, const std::vector<AABB>& aabbs, std::vector<int> sphereIndices) {
    BVHNode node;

    // Compute bounding box for all spheres in this node
    AABB box = aabbs[sphereIndices[0]];
    for (int i = 1; i < sphereIndices.size(); i++) {
        box = surroundingBox(box, aabbs[sphereIndices[i]]);
    }

    node.aabb = box;

    if (sphereIndices.size() == 1) {
        node.sphereIndex = sphereIndices[0];
        node.left = node.right = -1;
        bvh.push_back(node);
        return bvh.size() - 1;
    }

    // Split: sort by axis
    int axis = rand() % 3;
    std::sort(sphereIndices.begin(), sphereIndices.end(), [&](int a, int b) {
        return aabbs[a].center()[axis] < aabbs[b].center()[axis];
    });

    int mid = sphereIndices.size() / 2;
    std::vector<int> leftIndices(sphereIndices.begin(), sphereIndices.begin() + mid);
    std::vector<int> rightIndices(sphereIndices.begin() + mid, sphereIndices.end());

    int leftChild = buildBVH(bvh, spheres, aabbs, leftIndices);
    int rightChild = buildBVH(bvh, spheres, aabbs, rightIndices);

    node.left = leftChild;
    node.right = rightChild;
    bvh.push_back(node);
    return bvh.size() - 1;
}

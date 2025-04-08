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

int findMaxVarianceAxis(const std::vector<int> &sphereIndices, const std::vector<AABB> &aabbs){
    float mean[3] = {0}, var[3] = {0};
    for (int idx : sphereIndices) {
        glm::vec3 c = aabbs[idx].center();
        mean[0] += c.x;
        mean[1] += c.y;
        mean[2] += c.z;
    }
    int n = sphereIndices.size();
    mean[0] /= n; mean[1] /= n; mean[2] /= n;

    for (int idx : sphereIndices) {
        glm::vec3 c = aabbs[idx].center();
        var[0] += (c.x - mean[0]) * (c.x - mean[0]);
        var[1] += (c.y - mean[1]) * (c.y - mean[1]);
        var[2] += (c.z - mean[2]) * (c.z - mean[2]);
    }

    int axis = 0;
    if (var[1] > var[0]) axis = 1;
    if (var[2] > var[axis]) axis = 2;

    return axis;
}

int longestAxis(const AABB &box){
    glm::vec3 extent = box.max - box.min;
    int axis;
    if (extent.x > extent.y && extent.x > extent.z)
        axis = 0;
    else if (extent.y > extent.z)
        axis = 1;
    else
        axis = 2;

    return axis;
}

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
    // int axis = findMaxVarianceAxis(sphereIndices, aabbs);
    // int axis = longestAxis(box);
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

int flattenBVH(int nodeIndex, const std::vector<BVHNode>& nodes, std::vector<BVHNodeFlat>& flatNodes, int nextAfterSubtree) {
    if (nodeIndex < 0) return nextAfterSubtree;

    const BVHNode& node = nodes[nodeIndex];
    int currentIndex = static_cast<int>(flatNodes.size());
    flatNodes.emplace_back();

    
    // Leaf node
    if (node.sphereIndex != -1) {
        BVHNodeFlat& flat = flatNodes[currentIndex];
        flat.aabbMin = glm::vec4(node.aabb.min, 0.0f);
        flat.aabbMax = glm::vec4(node.aabb.max, 0.0f);
        flat.meta = glm::ivec4(-1, -1, node.sphereIndex, nextAfterSubtree);
        return currentIndex;
    }

    // Internal node
    int rightFlatIndex = flattenBVH(node.right, nodes, flatNodes, nextAfterSubtree);
    int leftFlatIndex = flattenBVH(node.left, nodes, flatNodes, rightFlatIndex);

    BVHNodeFlat& flat = flatNodes[currentIndex];
    flat.aabbMin = glm::vec4(node.aabb.min, 0.0f);
    flat.aabbMax = glm::vec4(node.aabb.max, 0.0f);
    flat.meta = glm::ivec4(leftFlatIndex, rightFlatIndex, -1, nextAfterSubtree);

    return currentIndex;
}

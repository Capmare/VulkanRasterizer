//
// Created by capma on 8/8/2025.
//

#ifndef MATH_H
#define MATH_H

#include <glm/glm.hpp>
#include <array>

#include "Structs/Mesh.h"

class VulkanMath {
public:
    static std::array<glm::vec3,8> GetAABBCorners(const glm::vec3& mn, const glm::vec3& mx) {
        return {
            glm::vec3(mn.x, mn.y, mn.z),
            glm::vec3(mx.x, mn.y, mn.z),
            glm::vec3(mn.x, mx.y, mn.z),
            glm::vec3(mx.x, mx.y, mn.z),
            glm::vec3(mn.x, mn.y, mx.z),
            glm::vec3(mx.x, mn.y, mx.z),
            glm::vec3(mn.x, mx.y, mx.z),
            glm::vec3(mx.x, mx.y, mx.z)
        };
    }

    static std::pair<glm::vec3, glm::vec3> ComputeSceneAABB(const std::vector<Mesh>& Meshes) {
        glm::vec3 outMin = glm::vec3(std::numeric_limits<float>::infinity());
        glm::vec3 outMax = glm::vec3(-std::numeric_limits<float>::infinity());

        for (const auto& mesh : Meshes)
        {
            for (const glm::vec3& pos : mesh.m_Positions)
            {
                outMin = glm::min(outMin, pos);
                outMax = glm::max(outMax, pos);
            }
        }

        return { outMin, outMax };
    }

};



#endif //MATH_H

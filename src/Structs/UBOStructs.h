//
// Created by capma on 8/8/2025.
//
#pragma once

#include <glm/glm.hpp>


struct alignas(16)  MVP {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 cameraPos;
};

struct alignas(16)  ShadowMVP {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};
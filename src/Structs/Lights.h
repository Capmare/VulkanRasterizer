//
// Created by capma on 8/6/2025.
//

#ifndef LIGHTS_H
#define LIGHTS_H

#include "glm/glm.hpp"

struct PointLight {
    glm::vec4 Position;
    glm::vec4 Color;
};

struct DirectionalLight {
    glm::vec4 Direction;
    glm::vec4 Color; // w is intensity
};



#endif //LIGHTS_H

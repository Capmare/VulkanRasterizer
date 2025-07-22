//
// Created by capma on 7/22/2025.
//

#ifndef CAMERA_H
#define CAMERA_H
#include <glm/gtc/matrix_transform.hpp>


class Camera {


public:
    Camera() = default;
    virtual ~Camera() = default;

    Camera(const Camera&) = delete;
    Camera(Camera&&) noexcept = delete;
    Camera& operator=(const Camera&) = delete;
    Camera& operator=(Camera&&) noexcept = delete;

    glm::mat4 GetViewMatrix() const;

    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    glm::vec3 position = glm::vec3(20.f, 10.f, 20.f);
    glm::vec3 target = glm::vec3(0.f, 0.f, 0.f);
    glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);
private:


    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};



#endif //CAMERA_H

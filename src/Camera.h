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

    float yaw = -90.0f;
    float pitch = 0.0f;

    void UpdateTarget() {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        target = glm::normalize(front);
    }

    void Move(const glm::vec3& offset) {
        position += offset;
    }


    glm::vec3 position = glm::vec3(0.f, 4.f, 0.f);
    glm::vec3 target = glm::vec3(0.f, 0.f, 0.f);
    glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);
private:


    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};



#endif //CAMERA_H

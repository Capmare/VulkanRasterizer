//
// Created by capma on 7/22/2025.
//

#include "Camera.h"

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(position, position + target, up);
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    proj[1][1] *= -1; // Vulkan clip space correction
    return proj;
}

ShadowMVP Camera::BuildDirectionalShadowCamera(const glm::vec3 &aabbMin, const glm::vec3 &aabbMax,
                                               const glm::vec3 &lightDirNormalized, const glm::vec3 &worldUp) {
    auto corners = VulkanMath::GetAABBCorners(aabbMin, aabbMax);
    glm::vec3 sceneCenter = 0.5f * (aabbMin + aabbMax);

    // 1) Project corners onto light direction to find min/max distances along light axis.
    float minProj = std::numeric_limits<float>::infinity();
    float maxProj = -std::numeric_limits<float>::infinity();
    for (const auto &c: corners) {
        float d = glm::dot(c, lightDirNormalized);
        minProj = std::min(minProj, d);
        maxProj = std::max(maxProj, d);
    }
    // depth range along light direction that covers the AABB:
    float depthRange = maxProj - minProj;
    (void)depthRange;

    // Choose view position so the light looks at the scene center and covers min max
    float midProj = 0.5f * (minProj + maxProj);
    glm::vec3 cameraCenter = sceneCenter - lightDirNormalized * midProj;

    // pick a stable up vector (avoid collinearity)
    glm::vec3 up = worldUp;
    if (glm::abs(glm::dot(up, lightDirNormalized)) > 0.999f) {
        // if nearly parallel, pick another up
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::mat4 view = glm::lookAt(
        cameraCenter, // eye
        sceneCenter, // center
        up // up
    );

    // Transform AABB corners into light (view) space, build orthographic bounds
    glm::vec3 minLS(std::numeric_limits<float>::infinity());
    glm::vec3 maxLS(-std::numeric_limits<float>::infinity());
    for (const auto &c: corners) {
        glm::vec4 lc = view * glm::vec4(c, 1.0f); // light-space
        minLS = glm::min(minLS, glm::vec3(lc));
        maxLS = glm::max(maxLS, glm::vec3(lc));
    }

    // orthographic extents in light-space
    float left = minLS.x;
    float right = maxLS.x;
    float bottom = minLS.y;
    float top = maxLS.y;

    float nearZ = minLS.z;
    float farZ = maxLS.z;
    if (nearZ > farZ) std::swap(nearZ, farZ);

    const float margin = 0.01f;
    left -= margin;
    right += margin;
    bottom -= margin;
    top += margin;

    glm::mat4 proj = glm::ortho(left, right, bottom, top, nearZ, farZ);

    ShadowMVP out;
    out.view = view;
    out.proj = proj;
    out.vp = proj * view;
    out.center = sceneCenter;
    out.NearFarPlanes[0] = nearZ;
    out.NearFarPlanes[1] = farZ;
    return out;
}

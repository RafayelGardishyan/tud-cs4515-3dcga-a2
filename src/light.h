#pragma once

#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()

// Basic light structure - expand with additional properties as needed
struct RS_Light {
    glm::vec3 position { 0.0f, 2.0f, 0.0f };
    glm::vec3 color { 1.0f, 1.0f, 1.0f };
    float intensity { 1.0f };

    // TODO: Add additional properties:
    // - Light type (point, directional, spot)
    // - Spot light parameters (direction, fov)
    // - Shadow map parameters
    // etc.
};
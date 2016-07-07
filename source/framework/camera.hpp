
// ================================================================================================
// -*- C++ -*-
// File: camera.hpp
// Author: Guilherme R. Lampert
// Created on: 05/07/16
// Brief: Simple First Person 3D camera for the demos.
// ================================================================================================

#ifndef CAMERA_HPP
#define CAMERA_HPP

// ========================================================
// class Camera:
// ========================================================

class Camera final
{
public:

    //
    // First person camera axes:
    //
    //    (up)
    //    +Y   +Z (forward)
    //    |   /
    //    |  /
    //    | /
    //    + ------ +X (right)
    //  (eye)
    //
    Vec3 right;
    Vec3 up;
    Vec3 forward;
    Vec3 eye;
    Mat4 viewMatrix;
    Mat4 projMatrix;
    Mat4 vpMatrix;

    float rotateSpeed   = 8.0f;  // Mouse rotation.
    float moveSpeed     = 20.0f; // Camera movement.
    float maxPitchAngle = 89.5f; // Max degrees of rotation to avoid lock.
    float pitchAmount   = 0.0f;  // Stored from latest update.
    float fovYDegrees   = 0.0f;  // Set via constructor or adjustFov().

    enum MoveDir
    {
        Forward, // Move forward relative to the camera's space
        Back,    // Move backward relative to the camera's space
        Left,    // Move left relative to the camera's space
        Right    // Move right relative to the camera's space
    };

    Camera(const float scrWidth, const float scrHeight,
           const float fovYDegs, const float zNear, const float zFar)
    {
        right       = Vec3{ 1.0f, 0.0f, 0.0f };
        up          = Vec3{ 0.0f, 1.0f, 0.0f };
        forward     = Vec3{ 0.0f, 0.0f, 1.0f };
        eye         = Vec3{ 0.0f, 0.0f, 0.0f };
        vpMatrix    = Mat4::identity();
        viewMatrix  = Mat4::identity();
        adjustFov(scrWidth, scrHeight, fovYDegs, zNear, zFar);
    }

    void adjustFov(const float scrWidth, const float scrHeight,
                   const float fovYDegs, const float zNear, const float zFar)
    {
        projMatrix  = Mat4::perspective(degToRad(fovYDegs), (scrWidth / scrHeight), zNear, zFar);
        fovYDegrees = fovYDegs;
    }

    void pitch(const float angle)
    {
        // Pitches camera by 'angle' radians.
        forward = rotateAroundAxis(forward, right, angle); // Calculate new forward.
        up      = cross(forward, right);                   // Calculate new camera up vector.
    }

    void rotate(const float angle)
    {
        // Rotates around world Y-axis by the given angle (in radians).
        const float sinAng = std::sin(angle);
        const float cosAng = std::cos(angle);
        float xxx, zzz;

        // Rotate forward vector:
        xxx = forward[0];
        zzz = forward[2];
        forward[0] = xxx *  cosAng + zzz * sinAng;
        forward[2] = xxx * -sinAng + zzz * cosAng;

        // Rotate up vector:
        xxx = up[0];
        zzz = up[2];
        up[0] = xxx *  cosAng + zzz * sinAng;
        up[2] = xxx * -sinAng + zzz * cosAng;

        // Rotate right vector:
        xxx = right[0];
        zzz = right[2];
        right[0] = xxx *  cosAng + zzz * sinAng;
        right[2] = xxx * -sinAng + zzz * cosAng;
    }

    void move(const MoveDir dir, const float amount)
    {
        switch (dir)
        {
        case Camera::Forward : eye += forward * amount; break;
        case Camera::Back    : eye -= forward * amount; break;
        case Camera::Left    : eye += right   * amount; break;
        case Camera::Right   : eye -= right   * amount; break;
        } // switch (dir)
    }

    void checkKeyboardMovement(const bool wDown, const bool sDown,
                               const bool aDown, const bool dDown,
                               const double deltaSeconds)
    {
        if (aDown) { move(Camera::Left,    moveSpeed * deltaSeconds); }
        if (dDown) { move(Camera::Right,   moveSpeed * deltaSeconds); }
        if (wDown) { move(Camera::Forward, moveSpeed * deltaSeconds); }
        if (sDown) { move(Camera::Back,    moveSpeed * deltaSeconds); }
    }

    void checkMouseRotation(const float mouseDeltaX, const float mouseDeltaY, const double deltaSeconds)
    {
        // Rotate left/right:
        float amt = mouseDeltaX * rotateSpeed * deltaSeconds;
        rotate(degToRad(-amt));

        // Calculate amount to rotate up/down:
        amt = mouseDeltaY * rotateSpeed * deltaSeconds;

        // Clamp pitch amount:
        if ((pitchAmount + amt) <= -maxPitchAngle)
        {
            amt = -maxPitchAngle - pitchAmount;
            pitchAmount = -maxPitchAngle;
        }
        else if ((pitchAmount + amt) >= maxPitchAngle)
        {
            amt = maxPitchAngle - pitchAmount;
            pitchAmount = maxPitchAngle;
        }
        else
        {
            pitchAmount += amt;
        }

        pitch(degToRad(-amt));
    }

    void updateMatrices()
    {
        viewMatrix = Mat4::lookAt(Point3(eye), getTarget(), up);
        vpMatrix   = projMatrix * viewMatrix; // Vectormath lib uses column-major OGL style, so multiply P*V*M
    }

    Point3 getTarget() const
    {
        return { eye[0] + forward[0], eye[1] + forward[1], eye[2] + forward[2] };
    }

    static Vec3 rotateAroundAxis(const Vec3 & vec, const Vec3 & axis, const float angle)
    {
        const float sinAng = std::sin(angle);
        const float cosAng = std::cos(angle);
        const float oneMinusCosAng = (1.0f - cosAng);

        const float aX = axis[0];
        const float aY = axis[1];
        const float aZ = axis[2];

        float x = (aX * aX * oneMinusCosAng + cosAng)      * vec[0] +
                  (aX * aY * oneMinusCosAng + aZ * sinAng) * vec[1] +
                  (aX * aZ * oneMinusCosAng - aY * sinAng) * vec[2];

        float y = (aX * aY * oneMinusCosAng - aZ * sinAng) * vec[0] +
                  (aY * aY * oneMinusCosAng + cosAng)      * vec[1] +
                  (aY * aZ * oneMinusCosAng + aX * sinAng) * vec[2];

        float z = (aX * aZ * oneMinusCosAng + aY * sinAng) * vec[0] +
                  (aY * aZ * oneMinusCosAng - aX * sinAng) * vec[1] +
                  (aZ * aZ * oneMinusCosAng + cosAng)      * vec[2];

        return { x, y, z };
    }
};

#endif // CAMERA_HPP

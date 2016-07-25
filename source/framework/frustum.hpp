
// ================================================================================================
// -*- C++ -*-
// File: frustum.hpp
// Author: Guilherme R. Lampert
// Created on: 25/07/16
// Brief: Frustum class with methods for intersection test with common shapes.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#ifndef FRUSTUM_HPP
#define FRUSTUM_HPP

#include "vectormath.hpp"

// ========================================================
// class Frustum:
// ========================================================

class Frustum final
{
public:

    // Frustum planes:
    enum { A, B, C, D };
    float p[6][4];

    // projection * view:
    Mat4 clipMatrix;

    // Sets everything to zero / identity.
    Frustum() noexcept;

    // Construct from camera matrices.
    Frustum(const Mat4 & view, const Mat4 & projection);

    // Compute frustum planes from camera matrices. Also sets 'clipMatrix' by multiplying projection * view.
    void computeClippingPlanes(const Mat4 & view, const Mat4 & projection);

    //
    // Bounding volume => frustum testing:
    //

    // Point:
    bool testPoint(const float x, const float y, const float z) const;
    bool testPoint(const Vec3 & v) const { return testPoint(v[0], v[1], v[2]); }

    // Bounding sphere:
    bool testSphere(const float x, const float y, const float z, const float radius) const;
    bool testSphere(const Vec3 & center, const float radius) const { return testSphere(center[0], center[1], center[2], radius); }

    // Cube/box:
    bool testCube(const float x, const float y, const float z, const float size) const;
    bool testCube(const Vec3 & center, const float size) const { return testCube(center[0], center[1], center[2], size); }

    // Axis-aligned bounding box. True if box is partly intersecting or fully contained in the frustum.
    bool testAabb(const Vec3 & mins, const Vec3 & maxs) const;
};

#endif // FRUSTUM_HPP

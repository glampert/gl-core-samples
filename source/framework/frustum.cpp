
// ================================================================================================
// -*- C++ -*-
// File: frustum.cpp
// Author: Guilherme R. Lampert
// Created on: 25/07/16
// Brief: Frustum class with methods for intersection test with common shapes.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#include "frustum.hpp"

static void normalizePlane(float p[4])
{
    // plane *= 1/sqrt(p.a * p.a + p.b * p.b + p.c * p.c);
    const float invLen = 1.0f / std::sqrt((p[0] * p[0]) + (p[1] * p[1]) + (p[2] * p[2]));
    p[0] *= invLen;
    p[1] *= invLen;
    p[2] *= invLen;
    p[3] *= invLen;
}

Frustum::Frustum() noexcept
    : clipMatrix{ Matrix4::identity() }
{
    for (int x = 0; x < 6; ++x)
    {
        for (int y = 0; y < 4; ++y)
        {
            p[x][y] = 0.0f;
        }
    }
}

Frustum::Frustum(const Matrix4 & view, const Matrix4 & projection)
{
    computeClippingPlanes(view, projection);
}

void Frustum::computeClippingPlanes(const Matrix4 & view, const Matrix4 & projection)
{
    // Compute a clip matrix:
    clipMatrix = projection * view;

    // Compute and normalize the 6 frustum planes:
    const float * m = toFloatPtr(clipMatrix);
    p[0][A] = m[3]  - m[0];
    p[0][B] = m[7]  - m[4];
    p[0][C] = m[11] - m[8];
    p[0][D] = m[15] - m[12];
    normalizePlane(p[0]);
    p[1][A] = m[3]  + m[0];
    p[1][B] = m[7]  + m[4];
    p[1][C] = m[11] + m[8];
    p[1][D] = m[15] + m[12];
    normalizePlane(p[1]);
    p[2][A] = m[3]  + m[1];
    p[2][B] = m[7]  + m[5];
    p[2][C] = m[11] + m[9];
    p[2][D] = m[15] + m[13];
    normalizePlane(p[2]);
    p[3][A] = m[3]  - m[1];
    p[3][B] = m[7]  - m[5];
    p[3][C] = m[11] - m[9];
    p[3][D] = m[15] - m[13];
    normalizePlane(p[3]);
    p[4][A] = m[3]  - m[2];
    p[4][B] = m[7]  - m[6];
    p[4][C] = m[11] - m[10];
    p[4][D] = m[15] - m[14];
    normalizePlane(p[4]);
    p[5][A] = m[3]  + m[2];
    p[5][B] = m[7]  + m[6];
    p[5][C] = m[11] + m[10];
    p[5][D] = m[15] + m[14];
    normalizePlane(p[5]);
}

bool Frustum::testPoint(const float x, const float y, const float z) const
{
    for (int i = 0; i < 6; ++i)
    {
        if ((p[i][A] * x + p[i][B] * y + p[i][C] * z + p[i][D]) <= 0.0f)
        {
            return false;
        }
    }
    return true;
}

bool Frustum::testSphere(const float x, const float y, const float z, const float radius) const
{
    for (int i = 0; i < 6; ++i)
    {
        if ((p[i][A] * x + p[i][B] * y + p[i][C] * z + p[i][D]) <= -radius)
        {
            return false;
        }
    }
    return true;
}

bool Frustum::testCube(const float x, const float y, const float z, const float size) const
{
    for (int i = 0; i < 6; ++i)
    {
        if ((p[i][A] * (x - size) + p[i][B] * (y - size) + p[i][C] * (z - size) + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * (x + size) + p[i][B] * (y - size) + p[i][C] * (z - size) + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * (x - size) + p[i][B] * (y + size) + p[i][C] * (z - size) + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * (x + size) + p[i][B] * (y + size) + p[i][C] * (z - size) + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * (x - size) + p[i][B] * (y - size) + p[i][C] * (z + size) + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * (x + size) + p[i][B] * (y - size) + p[i][C] * (z + size) + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * (x - size) + p[i][B] * (y + size) + p[i][C] * (z + size) + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * (x + size) + p[i][B] * (y + size) + p[i][C] * (z + size) + p[i][D]) > 0.0f)
        {
            continue;
        }
        return false;
    }
    return true;
}

bool Frustum::testAabb(const Vec3 & mins, const Vec3 & maxs) const
{
    for (int i = 0; i < 6; ++i)
    {
        if ((p[i][A] * mins[0] + p[i][B] * mins[1] + p[i][C] * mins[2] + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * maxs[0] + p[i][B] * mins[1] + p[i][C] * mins[2] + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * mins[0] + p[i][B] * maxs[1] + p[i][C] * mins[2] + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * maxs[0] + p[i][B] * maxs[1] + p[i][C] * mins[2] + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * mins[0] + p[i][B] * mins[1] + p[i][C] * maxs[2] + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * maxs[0] + p[i][B] * mins[1] + p[i][C] * maxs[2] + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * mins[0] + p[i][B] * maxs[1] + p[i][C] * maxs[2] + p[i][D]) > 0.0f)
        {
            continue;
        }
        if ((p[i][A] * maxs[0] + p[i][B] * maxs[1] + p[i][C] * maxs[2] + p[i][D]) > 0.0f)
        {
            continue;
        }
        return false;
    }
    return true;
}

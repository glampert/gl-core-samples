
// ================================================================================================
// -*- C++ -*-
// File: vectormath.hpp
// Author: Guilherme R. Lampert
// Created on: 15/11/15
// Brief: This header exposes the Sony Vector Math library types into the global scope.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#ifndef VECTORMATH_HPP
#define VECTORMATH_HPP

#include <cmath>
#include "vectormath/cpp/vectormath_aos.h"

// Shorter type names:
using Vec3 = Vectormath::Aos::Vector3;
using Vec4 = Vectormath::Aos::Vector4;
using Mat3 = Vectormath::Aos::Matrix3;
using Mat4 = Vectormath::Aos::Matrix4;

// Expose the global Vectormath functions:
using namespace Vectormath::Aos;

inline float * toFloatPtr(Point3 & p) { return reinterpret_cast<float *>(&p); }
inline float * toFloatPtr(Vec3 & v)   { return reinterpret_cast<float *>(&v); }
inline float * toFloatPtr(Vec4 & v)   { return reinterpret_cast<float *>(&v); }
inline float * toFloatPtr(Quat & q)   { return reinterpret_cast<float *>(&q); }
inline float * toFloatPtr(Mat3 & m)   { return reinterpret_cast<float *>(&m); }
inline float * toFloatPtr(Mat4 & m)   { return reinterpret_cast<float *>(&m); }

inline const float * toFloatPtr(const Point3 & p) { return reinterpret_cast<const float *>(&p); }
inline const float * toFloatPtr(const Vec3 & v)   { return reinterpret_cast<const float *>(&v); }
inline const float * toFloatPtr(const Vec4 & v)   { return reinterpret_cast<const float *>(&v); }
inline const float * toFloatPtr(const Quat & q)   { return reinterpret_cast<const float *>(&q); }
inline const float * toFloatPtr(const Mat3 & m)   { return reinterpret_cast<const float *>(&m); }
inline const float * toFloatPtr(const Mat4 & m)   { return reinterpret_cast<const float *>(&m); }

// Shorthand to discard the last element of a Vec4 and get a Point3.
inline Point3 toPoint3(const Vec4 & v4)
{
    return { v4[0], v4[1], v4[2] };
}

// Convert from world (global) coordinates to local model coordinates.
// Input matrix must be the inverse of the model matrix, e.g.: 'inverse(modelMatrix)'.
inline Point3 worldPointToModel(const Mat4 & invModelToWorldMatrix, const Point3 & point)
{
    return toPoint3(invModelToWorldMatrix * point);
}

#endif // VECTORMATH_HPP
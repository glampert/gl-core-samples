
// ================================================================================================
// -*- C++ -*-
// File: poly_triangulation.cpp
// Author: Guilherme R. Lampert
// Created on: 29/01/16
// Brief: Simple polygon triangulation tests.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#include "framework/gl_utils.hpp"

// ========================================================
// Helper Triangle struct with our vertex indexes:
// ========================================================

struct Triangle final
{
    Triangle() = default;
    Triangle(const GLDrawIndex v0, const GLDrawIndex v1, const GLDrawIndex v2)
    {
        vertexes[0] = v0;
        vertexes[1] = v1;
        vertexes[2] = v2;
    }

    GLDrawIndex vertexes[3];
};

// Type aliases just for syntactic sugar:
using TriangleList = std::vector<Triangle>;
using Polygon      = std::vector<Point3>;

// ========================================================
// Simple and fast triangulation for convex polygons:
// ========================================================

//
// Convex polygons can be easily triangulated using the algorithm below.
//
// A convex polygon is one with are no interior
// angles greater than 180 degrees. This excludes
// any kind of polygon with irregular shapes ("ears")
// or holes. Simple polygons like quadrilaterals, hexagons,
// diamonds, etc can be triangulated using this algorithm.
//
//  Create a stack with all of the vertexes in CW or CCW order;
//  Pop the top vertex and store in pStart;
//  Pop the top vertex and store in pHelper;
//  While stack is not empty:
//     Pop the top vertex and store in pTemp;
//     Create a triangle with pStart, pHelper, pTemp;
//     Let pHelper = pTemp;
//  End;
//
// As you can see, all triangles will shared the initial pStart vertex.
//
TriangleList triangulateConvexPolygon(const Polygon & polygon)
{
    const int vertexCount = polygon.size();
    TriangleList triangles;

    if (vertexCount < 3)
    {
        return triangles;
    }

    int cursor  = 0;
    int pTemp   = 0;
    int pStart  = cursor++;
    int pHelper = cursor++;

    while (cursor < vertexCount)
    {
        pTemp = cursor++;
        triangles.emplace_back(pStart, pHelper, pTemp);
        pHelper = pTemp;
    }

    return triangles;
}

// ========================================================
// Triangulation for complex or concave polygons:
// ========================================================

static int getNextActive(int x, const int vertexCount, const std::vector<bool> & active)
{
    for (;;)
    {
        if (++x == vertexCount)
        {
            x = 0;
        }
        if (active[x])
        {
            return x;
        }
    }
}

static int getPrevActive(int x, const int vertexCount, const std::vector<bool> & active)
{
    for (;;)
    {
        if (--x == -1)
        {
            x = vertexCount - 1;
        }
        if (active[x])
        {
            return x;
        }
    }
}

static bool testTriangle(const int pi1, const int pi2, const int pi3,
                         const Vec3 & p1, const Vec3 & p2, const Vec3 & p3,
                         const Vec3 & normal, const std::vector<bool> & active,
                         const Polygon & polygon, const float epsilon)
{
    bool result = false;
    const Vec3 n1 = cross(normal, normalize(p2 - p1));

    if (dot(n1, (p3 - p1)) > epsilon)
    {
        result = true;
        const Vec3 n2 = cross(normal, normalize(p3 - p2));
        const Vec3 n3 = cross(normal, normalize(p1 - p3));
        const int vertexCount = polygon.size();

        for (int v = 0; v < vertexCount; ++v)
        {
            // Look for other vertexes inside the triangle:
            if (active[v] && v != pi1 && v != pi2 && v != pi3)
            {
                const auto pv = Vec3{ polygon[v] };
                if (dot(n1, normalize(pv - p1)) > -epsilon &&
                    dot(n2, normalize(pv - p2)) > -epsilon &&
                    dot(n3, normalize(pv - p3)) > -epsilon)
                {
                    result = false;
                    break;
                }
            }
        }
    }

    return result;
}

//
// "Ear clipping"-based triangulation algorithm, adapted from the sample code
// presented in the "Mathematics for 3D Game Programming and Computer Graphics" book
// by Eric Lengyel (available at http://www.mathfor3dgameprogramming.com/code/Listing9.2.cpp).
//
// Outputs a list of n-2 triangles, where n is the number of vertexes in the input polygon.
// The normal vector of the plane where the polygon lies must be provided to correctly judge
// the winding order of the polygon.
//
// See also:
// https://en.wikipedia.org/wiki/Polygon_triangulation#Ear_clipping_method
//
TriangleList triangulateGenericPolygon(const Polygon & polygon, const Vec3 & normal, const float epsilon = 0.001f)
{
    const int vertexCount = polygon.size();
    TriangleList triangles;

    if (vertexCount < 3)
    {
        return triangles;
    }

    int start = 0;
    int p1 = 0;
    int p2 = 1;
    int m1 = vertexCount - 1;
    int m2 = vertexCount - 2;
    bool lastPositive = false;
    std::vector<bool> active(vertexCount, true);

    for (;;)
    {
        if (p2 == m2)
        {
            // Only three vertexes remain. We're done.
            triangles.emplace_back(m1, p1, p2);
            break;
        }

        // Expand the points to Vec3s:
        const auto vp1 = Vec3{ polygon[p1] };
        const auto vp2 = Vec3{ polygon[p2] };
        const auto vm1 = Vec3{ polygon[m1] };
        const auto vm2 = Vec3{ polygon[m2] };

        // Determine whether vp1, vp2, and vm1 form a valid triangle:
        bool positive = testTriangle(p1, p2, m1, vp2, vm1, vp1, normal, active, polygon, epsilon);

        // Determine whether vm1, vm2, and vp1 form a valid triangle:
        bool negative = testTriangle(m1, m2, p1, vp1, vm2, vm1, normal, active, polygon, epsilon);

        // If both triangles are valid, choose the
        // one having the larger smallest angle.
        if (positive && negative)
        {
            const float pDot = dot(normalize(vp2 - vm1), normalize(vm2 - vm1));
            const float mDot = dot(normalize(vm2 - vp1), normalize(vp2 - vp1));

            if (std::fabs(pDot - mDot) < epsilon)
            {
                if (lastPositive) { positive = false; }
                else              { negative = false; }
            }
            else
            {
                if (pDot < mDot)  { negative = false; }
                else              { positive = false; }
            }
        }

        if (positive)
        {
            // Output the triangle m1, p1, p2:
            active[p1] = false;
            triangles.emplace_back(m1, p1, p2);
            p1 = getNextActive(p1, vertexCount, active);
            p2 = getNextActive(p2, vertexCount, active);
            lastPositive = true;
            start = -1;
        }
        else if (negative)
        {
            // Output the triangle m2, m1, p1:
            active[m1] = false;
            triangles.emplace_back(m2, m1, p1);
            m1 = getPrevActive(m1, vertexCount, active);
            m2 = getPrevActive(m2, vertexCount, active);
            lastPositive = false;
            start = -1;
        }
        else // Not a valid triangle yet.
        {
            if (start == -1)
            {
                start = p2;
            }
            else if (p2 == start)
            {
                // Exit if we've gone all the way around the
                // polygon without finding a valid triangle.
                break;
            }

            // Advance working set of vertexes:
            m2 = m1;
            m1 = p1;
            p1 = p2;
            p2 = getNextActive(p2, vertexCount, active);
        }
    }

    return triangles;
}

// ========================================================
// Computing the normal of an arbitrary polygon:
// ========================================================

Vec3 computePolygonNormal(const Polygon & polygon)
{
    //
    // Computing the normal of an arbitrary polygon is
    // as simple as taking the cross product or each
    // pair of vertexes, from the first to the last
    // and wrapping around back to the first one if
    // needed. A more detailed mathematical explanation
    // of why this works can be found at:
    // http://www.iquilezles.org/www/articles/areas/areas.htm
    //

    Vec3 normal{ 0.0f, 0.0f, 0.0f };

    const int vertexCount = polygon.size();
    for (int v = 0; v < vertexCount; ++v)
    {
        const int vi0 = v;
        const int vi1 = (v + 1) % vertexCount;

        const auto p0 = Vec3{ polygon[vi0] };
        const auto p1 = Vec3{ polygon[vi1] };

        normal += cross(p0, p1);
    }

    return normalize(normal);
}

// ========================================================

// App constants:
constexpr int initialWinWidth  = 800;
constexpr int initialWinHeight = 600;
constexpr float defaultClearColor[]{ 0.1f, 0.1f, 0.1f, 1.0f };

// ========================================================
// class PolyTrisApp:
// ========================================================

class PolyTrisApp final
    : public GLFWApp
{
    GLBatchLineRenderer  lineRenderer  { *this, 1024 };
    GLBatchPointRenderer pointRenderer { *this, 128  };

    // Camera matrices (fixed):
    Mat4 projMatrix { Mat4::identity() };
    Mat4 viewMatrix { Mat4::identity() };
    Mat4 mvpMatrix  { Mat4::identity() };

    // The test polygon we are triangulating.
    Polygon polygon;

    // User interaction flags:
    int  shapeNumber      = 0;    // Cycle with left mouse click
    bool showTriangulated = true; // Toggle with right mouse click

public:

    PolyTrisApp();
    void onInit() override;
    void onFrameRender(std::int64_t currentTimeMillis, std::int64_t elapsedTimeMillis) override;
    void onMouseButton(MouseButton button, bool pressed) override;
    void updateShapes();
};

PolyTrisApp::PolyTrisApp()
    : GLFWApp(initialWinWidth, initialWinHeight, defaultClearColor, "OpenGL Polygon Triangulation demo")
{
    printF("---- PolyTrisApp starting up... ----");
}

void PolyTrisApp::onInit()
{
    viewMatrix = Mat4::lookAt(Point3{ 0.0f, 0.0f, 1.0f }, Point3{ 0.0f, 0.0f, -1.0f }, Vec3::yAxis());
    projMatrix = Mat4::perspective(degToRad(60.0f), aspectRatio(initialWinWidth, initialWinHeight), 0.5f, 1000.0f);
    mvpMatrix  = projMatrix * viewMatrix;

    updateShapes();
}

void PolyTrisApp::onFrameRender(const std::int64_t /* currentTimeMillis */,
                                const std::int64_t /* elapsedTimeMillis */)
{
    lineRenderer.setLinesMvpMatrix(mvpMatrix);
    lineRenderer.drawLines();

    pointRenderer.setPointsMvpMatrix(mvpMatrix);
    pointRenderer.drawPoints();
}

void PolyTrisApp::onMouseButton(const MouseButton button, const bool pressed)
{
    if (button == MouseButton::Left && pressed)
    {
        shapeNumber = (shapeNumber + 1) % 4; // Four sample shapes.
        updateShapes();
    }
    else if (button == MouseButton::Right && pressed)
    {
        showTriangulated = !showTriangulated;
        updateShapes();
    }
}

void PolyTrisApp::updateShapes(void)
{
    polygon.clear();
    lineRenderer.clear();
    pointRenderer.clear();

    if (shapeNumber == 0)
    {
        // octagon
        polygon.emplace_back(-1.0f,  2.0f, -5.0f);
        polygon.emplace_back(-2.0f,  1.0f, -5.0f);
        polygon.emplace_back(-2.0f, -1.0f, -5.0f);
        polygon.emplace_back(-1.0f, -2.0f, -5.0f);
        polygon.emplace_back( 1.0f, -2.0f, -5.0f);
        polygon.emplace_back( 2.0f, -1.0f, -5.0f);
        polygon.emplace_back( 2.0f,  1.0f, -5.0f);
        polygon.emplace_back( 1.0f,  2.0f, -5.0f);
    }
    else if (shapeNumber == 1)
    {
        // diamond
        polygon.emplace_back(-3.0f,  0.0f, -5.0f);
        polygon.emplace_back( 0.0f, -3.0f, -5.0f);
        polygon.emplace_back( 3.0f,  0.0f, -5.0f);
        polygon.emplace_back( 2.0f,  1.0f, -5.0f);
        polygon.emplace_back(-2.0f,  1.0f, -5.0f);
    }
    else if (shapeNumber == 2)
    {
        // box
        polygon.emplace_back(-1.0f,  1.0f, -5.0f);
        polygon.emplace_back(-1.0f, -1.0f, -5.0f);
        polygon.emplace_back( 1.0f, -1.0f, -5.0f);
        polygon.emplace_back( 1.0f,  1.0f, -5.0f);
    }
    else if (shapeNumber == 3)
    {
        // A complex shape with twists and turns
        polygon.emplace_back(-1.0f,  2.0f, -5.0f);
        polygon.emplace_back(-3.0f,  0.0f, -5.0f);
        polygon.emplace_back(-3.0f, -1.0f, -5.0f);
        polygon.emplace_back(-2.0f, -2.0f, -5.0f);
        polygon.emplace_back(-2.0f, -3.0f, -5.0f);
        polygon.emplace_back( 0.0f, -3.0f, -5.0f);
        polygon.emplace_back( 1.0f, -2.0f, -5.0f);
        polygon.emplace_back( 2.0f, -2.0f, -5.0f);
        polygon.emplace_back( 2.0f,  0.0f, -5.0f);
        polygon.emplace_back( 2.0f,  2.0f, -5.0f);
        polygon.emplace_back( 1.0f,  3.0f, -5.0f);
        polygon.emplace_back( 1.0f,  2.0f, -5.0f);
        polygon.emplace_back( 0.0f,  0.0f, -5.0f);
        polygon.emplace_back(-1.0f,  2.0f, -5.0f);
    }
    else
    {
        errorF("Invalid shape number: %d", shapeNumber);
    }

    if (showTriangulated)
    {
        // The simple triangulation method cannot handle the last polygon correctly.
        //auto triangles = triangulateConvexPolygon(polygon);

        // The more sophisticated algorithm can handle all the above shapes and it
        // also produces more evenly distributed triangles than the naive algorithm.
        const Vec3 polygonNormal = computePolygonNormal(polygon);
        auto triangles = triangulateGenericPolygon(polygon, polygonNormal);

        for (const auto & tri : triangles)
        {
            auto p0 = polygon[tri.vertexes[0]];
            auto p1 = polygon[tri.vertexes[1]];
            auto p2 = polygon[tri.vertexes[2]];

            lineRenderer.addLine(p0, p1, Vec4{ 0.0f, 1.0f, 0.0f, 1.0f });
            lineRenderer.addLine(p1, p2, Vec4{ 0.0f, 1.0f, 0.0f, 1.0f });
            lineRenderer.addLine(p2, p0, Vec4{ 0.0f, 1.0f, 0.0f, 1.0f });

            pointRenderer.addPoint(p0, 20.0f, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f });
            pointRenderer.addPoint(p1, 20.0f, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f });
            pointRenderer.addPoint(p2, 20.0f, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }
    else // Just displays the outline of the shape
    {
        for (std::size_t i = 0; i < polygon.size(); ++i)
        {
            auto from = i;
            auto to = (i + 1) % polygon.size();

            lineRenderer.addLine(polygon[from], polygon[to], Vec4{ 0.0f, 1.0f, 0.0f, 1.0f });
            pointRenderer.addPoint(polygon[from], 20.0f, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }
}

// ========================================================

GLFWApp::Ptr AppFactory::createGLFWAppInstance()
{
    return GLFWApp::Ptr{ new PolyTrisApp() };
}

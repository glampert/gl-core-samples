
// ================================================================================================
// -*- C++ -*-
// File: world_rendering.hpp
// Author: Guilherme R. Lampert
// Created on: 25/07/16
// Brief: World rendering and culling using Binary Space Partitioning and Portals.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#ifndef WORLD_RENDERING_HPP
#define WORLD_RENDERING_HPP

#include "framework/pool.hpp"
#include "framework/frustum.hpp"
#include "framework/linked_list.hpp"
#include "framework/gl_utils.hpp"

namespace World
{

// ========================================================
// Helper space-partitioning structures:
// ========================================================

struct Polygon;
struct BspNode;

struct Bounds final
{
    // Axis-aligned bounding box.
    Vec3 mins;
    Vec3 maxs;
};

struct Triangle final
{
    float verts[3][5]; // xyz uv
};

enum PlaneClassification
{
    OnPlane,
    BackSide,
    FrontSide,
    Spanning // Polygons only
};

struct Plane final
{
    static constexpr float Epsilon = 0.001f;

    Vec3  normal   = Vec3{ 0.0f, 0.0f, 0.0f };
    float distance = 0.0f;

    Plane & recalculateDistance(const Vec3 & point)
    {
        distance = -dot(point, normal);
        return *this;
    }

    Plane & fromPoints(const Vec3 & point1, const Vec3 & point2, const Vec3 & point3)
    {
        normal = normalize(cross(point2 - point1, point3 - point1));
        recalculateDistance(point1);
        return *this;
    }

    float getDistanceTo(const Vec3 & point) const
    {
        return dot(point, normal) + distance;
    }

    // Returns OnPlane, BackSide or FrontSide. Spanning only returned for the polygon/triangle checks.
    PlaneClassification classifyPoint(const Vec3 & point) const;

    // Generic version for N points (represented as Vec3s).
    PlaneClassification classifyPoints(const Vec3 * points, int count) const;

    // Any polygon topology.
    PlaneClassification classifyPolygon(const Polygon & poly,
                                        const std::vector<GLDrawVertex> & vertexes) const;

    // Triangulated polygons only.
    void classifyTriangleVerts(const Polygon & poly,
                               const std::vector<GLDrawVertex> & vertexes,
                               PlaneClassification outClassifications[3]) const;
};

struct Polygon final
{
    // Plane defined by this polygon.
    Plane plane;

    //
    // Offsets in to the RenderData vertex array:
    //
    int  firstVertex = 0; // First vertex in the world's vertex list.
    int  vertexCount = 0; // Number of verts for this polygon, starting from firstVertex.
    bool isTriangle() const noexcept { return vertexCount == 3; }

    //
    // Required by the LinkedList (PolygonList):
    //
    Polygon * next = nullptr;
    Polygon * prev = nullptr;
    bool isLinked() const noexcept { return prev != nullptr && next != nullptr; }

    //
    // Simpler way of accessing the nth vertex
    // from the vertex array this polygon points to:
    //
    GLDrawVertex getDrawVertex(const int index, const std::vector<GLDrawVertex> & vertexes) const
    {
        assert(index >= 0);
        assert(firstVertex + index < static_cast<int>(vertexes.size()));
        return vertexes[firstVertex + index];
    }
    Vec3 getVertexCoord(const int index, const std::vector<GLDrawVertex> & vertexes) const
    {
        assert(index >= 0);
        assert(firstVertex + index < static_cast<int>(vertexes.size()));
        const GLDrawVertex & dv = vertexes[firstVertex + index];
        return { dv.px, dv.py, dv.pz };
    }
};

using PolygonList = LinkedList<Polygon>;
using PolygonPool = Pool<Polygon, 256>;

struct Portal final
{
    Plane plane;

    static constexpr int MaxVerts = 8;
    Vec3 verts[MaxVerts];
    int  vertexCount = 0;
    int  id          = 0;

    BspNode * frontLeaf = nullptr;
    BspNode * backLeaf  = nullptr;

    //
    // Required by the LinkedList (PortalList):
    //
    Portal * next = nullptr;
    Portal * prev = nullptr;
    bool isLinked() const noexcept { return prev != nullptr && next != nullptr; }
};

using PortalList = LinkedList<Portal>;
using PortalPool = Pool<Portal, 64>;

struct BspNode final
{
    Plane       partition;
    PolygonList polygons;
    PortalList  portals;
    BspNode *   frontNode = nullptr;
    BspNode *   backNode  = nullptr;
    int         id        = 0; // Either the leaf number or partition number; Starts at 1, 0 reserved for uninitialized.
    int         visFrame  = 0; // Node is visible if visFrame == g_nFrameNumber.
    bool        isLeaf    = false;
};

using BspNodePool = Pool<BspNode, 256>;

// ========================================================
// World RenderData:
// ========================================================

class RenderData final
{
public:

    // OpenGL render data:
    std::vector<GLDrawVertex> vertexes;    // System-memory copy
    GLVertexArray             vertexArray; // GPU-memory copy
    GLShaderProg              mainShader;
    GLint                     mainBaseTextureLocation;
    GLint                     mainMvpMatrixLocation;
    GLint                     mainModelViewMatrixLocation;
    GLint                     mainRenderOutlineLocation;
    GLTexture                 debugTexture;
    GLShaderProg              debugPortalsShader;
    GLint                     debugPortalsBaseTextureLocation;
    GLint                     debugPortalsMvpMatrixLocation;
    int                       debugFirstPortalVert;
    int                       debugPortalsVertCount;

    // Memory pools:
    PolygonPool               polygonPoolAlloc;
    BspNodePool               bspNodePoolAlloc;
    PortalPool                portalPoolAlloc;

    // The BSP tree:
    BspNode *                 bspRoot;
    int                       bspPortalCount;
    int                       bspLeafCount;
    int                       bspPartitionCount;
    std::vector<BspNode *>    bspPartitionNodes;
    std::vector<BspNode *>    bspLeafNodes;
    Bounds                    bounds;

    explicit RenderData(GLFWApp & owner);
    RenderData(const RenderData & other) = delete; // Not copyable.
    RenderData & operator = (const RenderData & other) = delete;

    //
    // Vertex allocation:
    //
    void addVertex(const GLDrawVertex & vert)
    {
        vertexes.push_back(vert);
    }
    int getVertexCount() const noexcept
    {
        return static_cast<int>(vertexes.size());
    }
    void preAllocVertexes(const int count)
    {
        if (count > 0)
        {
            vertexes.reserve(count);
        }
    }

    //
    // BSP node allocation:
    //
    BspNode * allocBspNode()
    {
        auto node = bspNodePoolAlloc.allocate();
        return construct(node);
    }
    void freeBspNode(BspNode * node)
    {
        bspNodePoolAlloc.deallocate(node);
    }

    //
    // Polygon allocation:
    //
    Polygon * allocPolygon()
    {
        auto poly = polygonPoolAlloc.allocate();
        return construct(poly);
    }
    void freePolygon(Polygon * poly)
    {
        polygonPoolAlloc.deallocate(poly);
    }

    //
    // Portal allocation:
    //
    Portal * allocPortal()
    {
        auto portal = portalPoolAlloc.allocate();
        return construct(portal);
    }
    void freePortal(Portal * portal)
    {
        portalPoolAlloc.deallocate(portal);
    }
    Portal * clonePortal(const Portal & copy)
    {
        auto newPortal       = allocPortal();
        *newPortal           = copy;
        newPortal->next      = nullptr;
        newPortal->prev      = nullptr;
        newPortal->frontLeaf = nullptr;
        newPortal->backLeaf  = nullptr;
        return newPortal;
    }

    //
    // Miscellaneous:
    //
    void submitGLVertexArray();
    void loadShaders();
    void loadTextures();
    void computeBounds();
    void cleanup();
};

// World loading:
void createFromPolygons(RenderData * world, const Triangle * worldPolys, int worldPolyCount);
bool createFromDatafile(RenderData * world, const char * filename, float scale = 1.0f);

// Potentially Visible Set (PVS):
int countVisibleLeaves(const RenderData & world);
BspNode * findLeafRecursive(const Vec3 & referencePosition, BspNode * node);
void computePotentiallyVisibleSet(const Vec3 & eye, const Frustum & frustum, BspNode * currentLeaf);

// World rendering:
void render(RenderData * world, const Vec3 & eyePosition, const Mat4 & viewMatrix, const Mat4 & mvpMatrix);

// ========================================================
// Global configuration parameters and debug counters:
// ========================================================

// On/off switches:
extern bool g_bBuildBspTree; // Only applicable while loading a world map.
extern bool g_bRenderUseBsp;
extern bool g_bRenderWithDepthTest;
extern bool g_bRenderDebugPortals;
extern bool g_bRenderWorldWrireframe;
extern bool g_bRenderWorldSolid;

// Debug stats counters:
extern int g_nPolysOnPlane;
extern int g_nPolysBackSide;
extern int g_nPolysFrontSide;
extern int g_nPolysSpanning;
extern int g_nPolysRendered;
extern int g_nPolyListsRendered;
extern int g_nFrameNumber;

} // namespace World {}

#endif // WORLD_RENDERING_HPP

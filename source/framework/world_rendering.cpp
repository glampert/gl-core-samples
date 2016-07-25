
// ================================================================================================
// -*- C++ -*-
// File: world_rendering.cpp
// Author: Guilherme R. Lampert
// Created on: 25/07/16
// Brief: World rendering and culling using Binary Space Partitioning and Portals.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#include "world_rendering.hpp"
#include <cstdio>

//
// Some of the code in this file was based on samples provided by Alan Baylis in his website:
//  http://www.alsprogrammingresource.com/portals.html
//  http://www.alsprogrammingresource.com/pvs_tutorial.html
//
// The "BSP FAQ" also has a lot of relevant information on the subject:
//  ftp://ftp.sgi.com/other/bspfaq/faq/bspfaq.txt
//

namespace World
{

// ========================================================
// Global configuration parameters and debug counters:
// ========================================================

bool g_bBuildBspTree          = true;
bool g_bRenderUseBsp          = true;
bool g_bRenderWithDepthTest   = true;
bool g_bRenderDebugPortals    = true;
bool g_bRenderWorldWrireframe = true;
bool g_bRenderWorldSolid      = true;

int g_nPolysOnPlane           = 0;
int g_nPolysBackSide          = 0;
int g_nPolysFrontSide         = 0;
int g_nPolysSpanning          = 0;
int g_nPolysRendered          = 0;
int g_nPolyListsRendered      = 0;
int g_nFrameNumber            = 0;

// ========================================================
// Plane methods:
// ========================================================

PlaneClassification Plane::classifyPoint(const Vec3 & point) const
{
    const float d = getDistanceTo(point);
    if (floatGreaterThanZero(d, Epsilon)) // (d > 0.0f)
    {
        return FrontSide;
    }
    else if (floatLessThanZero(d, Epsilon)) // (d < 0.0f)
    {
        return BackSide;
    }
    else // (d == 0.0f)
    {
        return OnPlane;
    }
}

PlaneClassification Plane::classifyPoints(const Vec3 * const points, const int count) const
{
    assert(points != nullptr);

    int classifications[3] = { 0, 0, 0 }; // OnPlane, BackSide, FrontSide
    for (int p = 0; p < count; ++p)
    {
        classifications[classifyPoint(points[p])]++;
    }

    if (classifications[OnPlane] == count)
    {
        return OnPlane;
    }
    else if (classifications[BackSide] == count)
    {
        return BackSide;
    }
    else if (classifications[FrontSide] == count)
    {
        return FrontSide;
    }
    else
    {
        // At least one point in the from and one in the back, so it is spanning the plane.
        if (classifications[BackSide] > 0 && classifications[FrontSide] > 0)
        {
            return Spanning;
        }
        else
        {
            // Some points are on the plane and some are in-front/behind,
            // so still consider the polygon to be on one side, not spanning.
            if (classifications[BackSide] > 0)
            {
                assert(classifications[BackSide] + classifications[OnPlane] == count);
                return BackSide;
            }
            else
            {
                assert(classifications[FrontSide] + classifications[OnPlane] == count);
                return FrontSide;
            }
        }
    }
}

PlaneClassification Plane::classifyPolygon(const Polygon & poly, const std::vector<GLDrawVertex> & vertexes) const
{
    assert(poly.firstVertex >= 0);
    assert(poly.firstVertex + poly.vertexCount <= static_cast<int>(vertexes.size()));

    int classifications[3] = { 0, 0, 0 }; // OnPlane, BackSide, FrontSide
    for (int v = 0; v < poly.vertexCount; ++v)
    {
        const GLDrawVertex & vert = vertexes[poly.firstVertex + v];
        const Vec3 p{ vert.px, vert.py, vert.pz };
        classifications[classifyPoint(p)]++;
    }

    if (classifications[OnPlane] == poly.vertexCount)
    {
        return OnPlane;
    }
    else if (classifications[BackSide] == poly.vertexCount)
    {
        return BackSide;
    }
    else if (classifications[FrontSide] == poly.vertexCount)
    {
        return FrontSide;
    }
    else
    {
        // At least one point in the from and one in the back, so it is spanning the plane.
        if (classifications[BackSide] > 0 && classifications[FrontSide] > 0)
        {
            return Spanning;
        }
        else
        {
            // Some points are on the plane and some are in-front/behind,
            // so still consider the polygon to be on one side, not spanning.
            if (classifications[BackSide] > 0)
            {
                assert(classifications[BackSide] + classifications[OnPlane] == poly.vertexCount);
                return BackSide;
            }
            else
            {
                assert(classifications[FrontSide] + classifications[OnPlane] == poly.vertexCount);
                return FrontSide;
            }
        }
    }
}

void Plane::classifyTriangleVerts(const Polygon & poly,
                                  const std::vector<GLDrawVertex> & vertexes,
                                  PlaneClassification outClassifications[3]) const
{
    assert(poly.isTriangle());
    assert(poly.firstVertex >= 0);
    assert(poly.firstVertex + poly.vertexCount <= static_cast<int>(vertexes.size()));

    for (int v = 0; v < poly.vertexCount; ++v)
    {
        const GLDrawVertex & vert = vertexes[poly.firstVertex + v];
        const Vec3 p{ vert.px, vert.py, vert.pz };
        outClassifications[v] = classifyPoint(p);
    }
}

// ========================================================
// World RenderData methods:
// ========================================================

RenderData::RenderData(GLFWApp & owner)
    : vertexArray                     { owner   }
    , mainShader                      { owner   }
    , mainBaseTextureLocation         { -1      }
    , mainMvpMatrixLocation           { -1      }
    , mainModelViewMatrixLocation     { -1      }
    , mainRenderOutlineLocation       { -1      }
    , debugTexture                    { owner   }
    , debugPortalsShader              { owner   }
    , debugPortalsBaseTextureLocation { -1      }
    , debugPortalsMvpMatrixLocation   { -1      }
    , debugFirstPortalVert            { 0       }
    , debugPortalsVertCount           { 0       }
    , bspRoot                         { nullptr }
    , bspPortalCount                  { 0       }
    , bspLeafCount                    { 0       }
    , bspPartitionCount               { 0       }
{ }

void RenderData::cleanup()
{
    // Reset to initial defaults:

    mainBaseTextureLocation         = -1;
    mainMvpMatrixLocation           = -1;
    mainModelViewMatrixLocation     = -1;
    mainRenderOutlineLocation       = -1;
    debugPortalsBaseTextureLocation = -1;
    debugPortalsMvpMatrixLocation   = -1;
    debugFirstPortalVert            = 0;
    debugPortalsVertCount           = 0;
    bspRoot                         = nullptr;
    bspPortalCount                  = 0;
    bspLeafCount                    = 0;
    bspPartitionCount               = 0;
    bounds.mins                     = Vec3{ 0.0f, 0.0f, 0.0f };
    bounds.maxs                     = Vec3{ 0.0f, 0.0f, 0.0f };

    vertexArray.cleanup();
    mainShader.cleanup();
    debugTexture.cleanup();
    debugPortalsShader.cleanup();

    polygonPoolAlloc.drain();
    bspNodePoolAlloc.drain();
    portalPoolAlloc.drain();

    vertexes.clear();
    bspLeafNodes.clear();
    bspPartitionNodes.clear();
}

void RenderData::submitGLVertexArray()
{
    // Unindexed drawing geometry for now.
    vertexArray.initFromData(vertexes.data(), vertexes.size(),
                             nullptr, 0, GL_STATIC_DRAW,
                             GLVertexLayout::Triangles);
}

void RenderData::loadShaders()
{
    // Main shader used by the level geometry:
    mainShader.initFromFiles("source/shaders/outline_flat.vert", "source/shaders/outline_flat.frag");
    mainModelViewMatrixLocation = mainShader.getUniformLocation("u_ModelViewMatrix");
    mainMvpMatrixLocation       = mainShader.getUniformLocation("u_MvpMatrix");
    mainBaseTextureLocation     = mainShader.getUniformLocation("u_BaseTexture");
    mainRenderOutlineLocation   = mainShader.getUniformLocation("u_RenderOutline");

    // Debug shader for the portal drawing:
    debugPortalsShader.initFromFiles("source/shaders/basic.vert", "source/shaders/basic.frag");
    debugPortalsMvpMatrixLocation   = debugPortalsShader.getUniformLocation("u_MvpMatrix");
    debugPortalsBaseTextureLocation = debugPortalsShader.getUniformLocation("u_BaseTexture");
}

void RenderData::loadTextures()
{
    // White and gray checkerboard pattern.
    const float texColorsDebug[][4]{
        { 0.2f, 0.2f, 0.2f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f }
    };
    debugTexture.initWithCheckerPattern(8, texColorsDebug, GLTexture::Filter::Nearest,
                                        0, GLTexture::WrapMode::Repeat);
}

void RenderData::computeBounds()
{
    if (vertexes.empty())
    {
        bounds.mins = Vec3{ 0.0f, 0.0f, 0.0f };
        bounds.maxs = bounds.mins;
        return;
    }

    bounds.mins = Vec3{  INFINITY,  INFINITY,  INFINITY };
    bounds.maxs = Vec3{ -INFINITY, -INFINITY, -INFINITY };
    for (const GLDrawVertex & vert : vertexes)
    {
        const Vec3 coord{ vert.px, vert.py, vert.pz };
        bounds.mins = minPerElem(bounds.mins, coord);
        bounds.maxs = maxPerElem(bounds.maxs, coord);
    }
}

// ========================================================
// Portal construction:
// ========================================================

Vec3 calcEdgeIntersection(const Vec3 & pointA, const Vec3 & pointB, const Plane & plane)
{
    const Vec3 diff = pointB - pointA;
    const float t = -plane.getDistanceTo(pointA) / dot(plane.normal, diff);
    return pointA + (diff * t);
}

void calcTexCoords(const Vec3 & pointA, const float uvA[2],
                   const Vec3 & pointB, const float uvB[2],
                   const Vec3 & intersectionPoint,
                   float outIntersectionUV[2])
{
    const Vec3  texVert1  = pointB - pointA;
    const Vec3  texVert2  = intersectionPoint - pointA;
    const float texScale  = length(texVert2) / length(texVert1);

    const float texVert1u = uvA[0];
    const float texVert2u = uvB[0];
    const float texVert1v = uvA[1];
    const float texVert2v = uvB[1];

    outIntersectionUV[0]  = texVert1u + ((texVert2u - texVert1u) * texScale);
    outIntersectionUV[1]  = texVert1v + ((texVert2v - texVert1v) * texScale);
}

PlaneClassification classifyPortal(const Portal & portal, const Plane & partition)
{
    assert(portal.vertexCount <= Portal::MaxVerts);
    return partition.classifyPoints(portal.verts, portal.vertexCount);
}

PlaneClassification classifyInvertedPortal(const Portal & portal, const Plane & partition)
{
    assert(portal.vertexCount <= Portal::MaxVerts);
    auto side = partition.classifyPoints(portal.verts, portal.vertexCount);

    if (side == OnPlane)
    {
        Vec3 portalNormal = portal.plane.normal;
        portalNormal *= -1.0f;

        side = partition.classifyPoint(portalNormal);
        assert(side != OnPlane);
    }

    return side;
}

void gatherBspNodeLists(BspNode * node, std::vector<BspNode *> * outPartitionNodes, std::vector<BspNode *> * outLeafNodes)
{
    assert(node              != nullptr);
    assert(outPartitionNodes != nullptr);
    assert(outLeafNodes      != nullptr);

    if (node->isLeaf)
    {
        outLeafNodes->push_back(node);
        return;
    }

    outPartitionNodes->push_back(node);
    gatherBspNodeLists(node->frontNode, outPartitionNodes, outLeafNodes);
    gatherBspNodeLists(node->backNode,  outPartitionNodes, outLeafNodes);
}

void makeLargePortal(const Bounds & worldBounds, const Plane & partition, Portal * outPortal)
{
    // Make large portal using planar mapping.
    enum { YZPlane, XZPlane, XYPlane } boxPlane;

    const Vec3  normal   = partition.normal;
    const float distance = partition.distance;

    // Find the primary axis plane:
    if (std::fabs(normal[0]) > std::fabs(normal[1]) &&
        std::fabs(normal[0]) > std::fabs(normal[2]))
    {
        boxPlane = YZPlane;
        outPortal->verts[0][1] = worldBounds.mins[1];
        outPortal->verts[0][2] = worldBounds.maxs[2];
        outPortal->verts[1][1] = worldBounds.mins[1];
        outPortal->verts[1][2] = worldBounds.mins[2];
        outPortal->verts[2][1] = worldBounds.maxs[1];
        outPortal->verts[2][2] = worldBounds.mins[2];
        outPortal->verts[3][1] = worldBounds.maxs[1];
        outPortal->verts[3][2] = worldBounds.maxs[2];
    }
    else if (std::fabs(normal[1]) > std::fabs(normal[0]) &&
             std::fabs(normal[1]) > std::fabs(normal[2]))
    {
        boxPlane = XZPlane;
        outPortal->verts[0][0] = worldBounds.mins[0];
        outPortal->verts[0][2] = worldBounds.maxs[2];
        outPortal->verts[1][0] = worldBounds.maxs[0];
        outPortal->verts[1][2] = worldBounds.maxs[2];
        outPortal->verts[2][0] = worldBounds.maxs[0];
        outPortal->verts[2][2] = worldBounds.mins[2];
        outPortal->verts[3][0] = worldBounds.mins[0];
        outPortal->verts[3][2] = worldBounds.mins[2];
    }
    else
    {
        boxPlane = XYPlane;
        outPortal->verts[0][0] = worldBounds.mins[0];
        outPortal->verts[0][1] = worldBounds.mins[1];
        outPortal->verts[1][0] = worldBounds.maxs[0];
        outPortal->verts[1][1] = worldBounds.mins[1];
        outPortal->verts[2][0] = worldBounds.maxs[0];
        outPortal->verts[2][1] = worldBounds.maxs[1];
        outPortal->verts[3][0] = worldBounds.mins[0];
        outPortal->verts[3][1] = worldBounds.maxs[1];
    }

    float X, Y, Z;
    switch (boxPlane)
    {
    case YZPlane :
        X = -(normal[1] * worldBounds.mins[1] + normal[2] * worldBounds.maxs[2] + distance) / normal[0];
        outPortal->verts[0][0] = X;
        X = -(normal[1] * worldBounds.mins[1] + normal[2] * worldBounds.mins[2] + distance) / normal[0];
        outPortal->verts[1][0] = X;
        X = -(normal[1] * worldBounds.maxs[1] + normal[2] * worldBounds.mins[2] + distance) / normal[0];
        outPortal->verts[2][0] = X;
        X = -(normal[1] * worldBounds.maxs[1] + normal[2] * worldBounds.maxs[2] + distance) / normal[0];
        outPortal->verts[3][0] = X;
        break;

    case XZPlane :
        Y = -(normal[0] * worldBounds.mins[0] + normal[2] * worldBounds.maxs[2] + distance) / normal[1];
        outPortal->verts[0][1] = Y;
        Y = -(normal[0] * worldBounds.maxs[0] + normal[2] * worldBounds.maxs[2] + distance) / normal[1];
        outPortal->verts[1][1] = Y;
        Y = -(normal[0] * worldBounds.maxs[0] + normal[2] * worldBounds.mins[2] + distance) / normal[1];
        outPortal->verts[2][1] = Y;
        Y = -(normal[0] * worldBounds.mins[0] + normal[2] * worldBounds.mins[2] + distance) / normal[1];
        outPortal->verts[3][1] = Y;
        break;

    case XYPlane :
        Z = -(normal[0] * worldBounds.mins[0] + normal[1] * worldBounds.mins[1] + distance) / normal[2];
        outPortal->verts[0][2] = Z;
        Z = -(normal[0] * worldBounds.maxs[0] + normal[1] * worldBounds.mins[1] + distance) / normal[2];
        outPortal->verts[1][2] = Z;
        Z = -(normal[0] * worldBounds.maxs[0] + normal[1] * worldBounds.maxs[1] + distance) / normal[2];
        outPortal->verts[2][2] = Z;
        Z = -(normal[0] * worldBounds.mins[0] + normal[1] * worldBounds.maxs[1] + distance) / normal[2];
        outPortal->verts[3][2] = Z;
        break;
    }

    outPortal->vertexCount = 4;
    outPortal->plane.fromPoints(outPortal->verts[0],
                                outPortal->verts[1],
                                outPortal->verts[2]);
}

bool splitPortal(const Portal * portalToSplit, const Plane & partition,
                 Portal * outFrontPortal, Portal * outBackPortal,
                 PlaneClassification * outPlaneSide = nullptr)
{
    assert(portalToSplit  != nullptr);
    assert(outFrontPortal != nullptr);
    assert(outBackPortal  != nullptr);

    const int vertexCount = portalToSplit->vertexCount;
    assert(vertexCount <= Portal::MaxVerts);

    Vec3 pointA;
    Vec3 pointB;
    PlaneClassification sideA;
    PlaneClassification sideB;

    int  frontCount = 0;
    int  backCount  = 0;
    Vec3 frontPoints[Portal::MaxVerts];
    Vec3 backPoints[Portal::MaxVerts];
    Vec3 intersectionPoint;

    const auto side = classifyPortal(*portalToSplit, partition);
    if (side != Spanning)
    {
        if (outPlaneSide != nullptr)
        {
            (*outPlaneSide) = side;
        }
        return false; // Portal not crossed by the partition plane.
    }

    // Try to split the portal:
    pointA = portalToSplit->verts[vertexCount - 1];
    sideA  = partition.classifyPoint(pointA);

    for (int v = -1; ++v < vertexCount;)
    {
        pointB = portalToSplit->verts[v];
        sideB  = partition.classifyPoint(pointB);

        if (sideB == FrontSide)
        {
            if (sideA == BackSide)
            {
                intersectionPoint = calcEdgeIntersection(pointA, pointB, partition);
                frontPoints[frontCount++] = intersectionPoint;
                backPoints[backCount++]   = intersectionPoint;
            }
            frontPoints[frontCount++] = pointB;
        }
        else if (sideB == BackSide)
        {
            if (sideA == FrontSide)
            {
                intersectionPoint = calcEdgeIntersection(pointA, pointB, partition);
                frontPoints[frontCount++] = intersectionPoint;
                backPoints[backCount++]   = intersectionPoint;
            }
            backPoints[backCount++] = pointB;
        }
        else
        {
            assert(sideB == OnPlane);
            frontPoints[frontCount++] = pointB;
            backPoints[backCount++]   = pointB;
        }

        pointA = pointB;
        sideA  = sideB;
    }

    assert(frontCount <= Portal::MaxVerts);
    assert(backCount  <= Portal::MaxVerts);
    assert(frontCount >= 3);
    assert(backCount  >= 3);

    for (int v = 0; v < frontCount; ++v)
    {
        outFrontPortal->verts[v] = frontPoints[v];
    }
    outFrontPortal->vertexCount = frontCount;
    outFrontPortal->plane.fromPoints(outFrontPortal->verts[0],
                                     outFrontPortal->verts[1],
                                     outFrontPortal->verts[2]);

    for (int v = 0; v < backCount; ++v)
    {
        outBackPortal->verts[v] = backPoints[v];
    }
    outBackPortal->vertexCount = backCount;
    outBackPortal->plane.fromPoints(outBackPortal->verts[0],
                                    outBackPortal->verts[1],
                                    outBackPortal->verts[2]);

    if (outPlaneSide != nullptr)
    {
        (*outPlaneSide) = side;
    }
    return true; // Portal was split.
}

void addPortalToBspNodeRecursive(RenderData * world, Portal * portal, BspNode * node)
{
    assert(world  != nullptr);
    assert(portal != nullptr);
    assert(node   != nullptr);

    if (node->isLeaf)
    {
        node->portals.push_back(portal);
        return;
    }

    const auto side = classifyPortal(*portal, node->partition);
    switch (side)
    {
    case OnPlane : // Add to both sides
        addPortalToBspNodeRecursive(world, portal, node->frontNode);
        addPortalToBspNodeRecursive(world, world->clonePortal(*portal), node->backNode);
        break;

    case BackSide :
        addPortalToBspNodeRecursive(world, portal, node->backNode);
        break;

    case FrontSide :
        addPortalToBspNodeRecursive(world, portal, node->frontNode);
        break;

    default : // Not supposed to be reached. Planes are already split.
        assert(false);
        break;
    } // switch (side)
}

void addPortalsToBspLeaves(RenderData * world, PortalList * allPortals)
{
    // Portals are now linked to the BSP nodes:
    Portal * portal;
    while ((portal = allPortals->pop_front()) != nullptr)
    {
        addPortalToBspNodeRecursive(world, portal, world->bspRoot);
    }

    assert(allPortals->empty());
}

void checkForSinglePortalsRecursive(BspNode * node, BspNode * originalNode, Portal * portal, int * outCount)
{
    if (node->isLeaf)
    {
        if (node->id != originalNode->id)
        {
            const Portal * tempPortal = node->portals.first();
            for (int i = node->portals.size(); i--; tempPortal = tempPortal->next)
            {
                if (tempPortal->id == portal->id)
                {
                    portal->frontLeaf = originalNode;
                    portal->backLeaf  = node;
                    (*outCount)      += 1;
                }
            }
        }
    }
    else
    {
        checkForSinglePortalsRecursive(node->frontNode, originalNode, portal, outCount);
        checkForSinglePortalsRecursive(node->backNode,  originalNode, portal, outCount);
    }
}

void clipPortalToLeaf(Portal * portal, const BspNode * leafNode)
{
    assert(portal   != nullptr);
    assert(leafNode != nullptr);
    assert(leafNode->isLeaf);

    const Polygon * poly = leafNode->polygons.first();
    for (int i = leafNode->polygons.size(); i--; poly = poly->next)
    {
        Portal frontPortal;
        Portal backPortal;

        const bool portalWasSplit = splitPortal(portal, poly->plane, &frontPortal, &backPortal);
        if (portalWasSplit)
        {
            for (int v = 0; v < frontPortal.vertexCount; ++v)
            {
                portal->verts[v] = frontPortal.verts[v];
            }

            portal->vertexCount = frontPortal.vertexCount;
            portal->plane       = frontPortal.plane;
        }
    }
}

void invertSinglePortal(Portal * portal)
{
    const Portal tempPortal = *portal;
    const int vertexCount   = portal->vertexCount;

    for (int v = 0; v < vertexCount; ++v)
    {
        portal->verts[v] = tempPortal.verts[(vertexCount - 1) - v];
    }

    assert(portal->vertexCount >= 3);
    portal->plane.fromPoints(portal->verts[0], portal->verts[1], portal->verts[2]);
}

void invertNodePortals(const RenderData & world, BspNode * node)
{
    Portal * portal = node->portals.first();
    for (int i = node->portals.size(); i--; portal = portal->next)
    {
        const Polygon * poly = node->polygons.first();
        for (int j = node->polygons.size(); j--; poly = poly->next)
        {
            assert(poly->isTriangle());
            PlaneClassification side;

            for (int v = 0; v < 3; ++v)
            {
                const Vec3 vert = poly->getVertexCoord(v, world.vertexes);
                side = portal->plane.classifyPoint(vert);

                if (side == BackSide)
                {
                    invertSinglePortal(portal);
                }

                if (side == FrontSide || side == BackSide)
                {
                    assert(portal->frontLeaf != nullptr);
                    if (portal->frontLeaf->id != node->id)
                    {
                        std::swap(portal->backLeaf, portal->frontLeaf);
                    }
                    break;
                }
            }

            if (side == FrontSide || side == BackSide)
            {
                break;
            }
        }
    }
}

bool removeExtraPortals(const Portal * portal)
{
    int frontCount = 0;
    const BspNode * backLeaf = portal->backLeaf;
    const Polygon * poly = backLeaf->polygons.first();

    for (int i = backLeaf->polygons.size(); i--; poly = poly->next)
    {
        if (classifyInvertedPortal(*portal, poly->plane) == FrontSide)
        {
            ++frontCount;
        }
    }

    return (frontCount != backLeaf->polygons.size());
}

void countPortalsRecursive(const BspNode * node, int * outCount)
{
    if (node == nullptr)
    {
        return;
    }

    (*outCount) += node->portals.size();

    countPortalsRecursive(node->frontNode, outCount);
    countPortalsRecursive(node->backNode,  outCount);
}

void findTruePortalsRecursive(RenderData * world, BspNode * node)
{
    assert(world != nullptr);
    assert(node  != nullptr);

    if (node->isLeaf)
    {
        Portal * portal = node->portals.first();
        int portalsLeft = node->portals.size();
        while (portalsLeft > 0)
        {
            int count = 0;
            checkForSinglePortalsRecursive(world->bspRoot, node, portal, &count);
            if (count == 0)
            {
                Portal * toRemove = portal;
                portal = portal->next;
                --portalsLeft;

                node->portals.remove(toRemove);
                world->freePortal(toRemove);
                continue;
            }

            clipPortalToLeaf(portal, portal->frontLeaf);
            clipPortalToLeaf(portal, portal->backLeaf);

            portal = portal->next;
            --portalsLeft;
        }

        // Also inverts the front and back leaf pointers if necessary.
        invertNodePortals(*world, node);

        portal = node->portals.first();
        portalsLeft = node->portals.size();
        while (portalsLeft > 0)
        {
            const bool shouldRemove = removeExtraPortals(portal);
            if (shouldRemove)
            {
                Portal * toRemove = portal;
                portal = portal->next;
                --portalsLeft;

                node->portals.remove(toRemove);
                world->freePortal(toRemove);
                continue;
            }

            portal = portal->next;
            --portalsLeft;
        }
    }
    else
    {
        findTruePortalsRecursive(world, node->frontNode);
        findTruePortalsRecursive(world, node->backNode);
    }
}

void buildPortals(RenderData * world)
{
    assert(world != nullptr);
    assert(world->bspRoot != nullptr);

    // Temporary list for the large portals
    PortalList allPortals;

    // Put the partition nodes and leaf nodes aside:
    gatherBspNodeLists(world->bspRoot, &world->bspPartitionNodes, &world->bspLeafNodes);

    // Create a large/rough portal for each partition:
    for (const BspNode * node : world->bspPartitionNodes)
    {
        Portal * newPortal = world->allocPortal();
        makeLargePortal(world->bounds, node->partition, newPortal);
        allPortals.push_back(newPortal);
    }

    Portal * portal;
    Portal * frontPortal = nullptr;
    Portal * backPortal  = nullptr;

    // Now the large portals are split into potential portals:
    for (const BspNode * node : world->bspPartitionNodes)
    {
        portal = allPortals.first();
        int portalsLeft = allPortals.size();

        while (portalsLeft > 0)
        {
            if (frontPortal == nullptr)
            {
                frontPortal = world->allocPortal();
                backPortal  = world->allocPortal();
            }

            const bool portalWasSplit = splitPortal(portal, node->partition, frontPortal, backPortal);
            if (portalWasSplit)
            {
                // NOTE: If we aren't passing these assertions, then the Plane::Epsilon
                // is too small and the planes should be made "thicker" with a bigger Epsilon.
                assert(classifyPortal(*frontPortal, node->partition) == FrontSide);
                assert(classifyPortal(*backPortal,  node->partition) == BackSide);

                allPortals.push_back(frontPortal);
                allPortals.push_back(backPortal);

                Portal * toRemove = portal;
                portal = portal->next;
                --portalsLeft;

                allPortals.remove(toRemove);
                world->freePortal(toRemove);

                frontPortal = nullptr;
                backPortal  = nullptr;

                continue;
            }

            portal = portal->next;
            --portalsLeft;
        }
    }

    if (frontPortal != nullptr)
    {
        world->freePortal(frontPortal);
        world->freePortal(backPortal);
    }

    // Loop through the portals and assign a unique id to each:
    int portalId = 0;
    portal = allPortals.first();
    for (int i = allPortals.size(); i--; portal = portal->next)
    {
        portal->id = ++portalId;
    }

    addPortalsToBspLeaves(world, &allPortals);
    findTruePortalsRecursive(world, world->bspRoot);

    // This is only used for debug display.
    world->bspPortalCount = 0;
    countPortalsRecursive(world->bspRoot, &world->bspPortalCount);
}

void triangulateConvexPolygon(const Vec3 * verts, const int vertexCount, std::vector<Vec3> * outTriangles)
{
    outTriangles->clear();

    int cursor  = 0;
    int pTemp   = 0;
    int pStart  = cursor++;
    int pHelper = cursor++;

    while (cursor < vertexCount)
    {
        pTemp = cursor++;
        outTriangles->push_back(verts[pStart]);
        outTriangles->push_back(verts[pHelper]);
        outTriangles->push_back(verts[pTemp]);
        pHelper = pTemp;
    }
}


void addDebugPortalsRecursive(RenderData * world, const BspNode * node,
                              int * outPortalVertsAdded, std::vector<int> * outPortalIdsAdded)
{
    if (node == nullptr)
    {
        return;
    }

    int nextPortalColor = 0;     // Each portal gets assigned a different color.
    std::vector<Vec3> triangles; // For portals that need to be triangulated.

    auto addTriangle = [world, &nextPortalColor](const Vec3 & a, const Vec3 & b, const Vec3 & c)
    {
        const Vec4 color = globalColorTable[nextPortalColor];
        GLDrawVertex vert{ 0.0f,0.0f,0.0f, 0.0f,0.0f,0.0f,
                           color[0], color[1], color[2], 0.5f,
                           1.0f,1.0f, 0.0f,0.0f,0.0f, 0.0f,0.0f,0.0f };
        vert.px = a[0];
        vert.py = a[1];
        vert.pz = a[2];
        world->addVertex(vert);
        vert.px = b[0];
        vert.py = b[1];
        vert.pz = b[2];
        world->addVertex(vert);
        vert.px = c[0];
        vert.py = c[1];
        vert.pz = c[2];
        world->addVertex(vert);
    };

    const Portal * portal = node->portals.first();
    for (int i = node->portals.size(); i--; portal = portal->next)
    {
        // Some portals might repeat. Don't add the vertexes twice.
        if (std::find(outPortalIdsAdded->begin(), outPortalIdsAdded->end(), portal->id) != outPortalIdsAdded->end())
        {
            continue;
        }
        outPortalIdsAdded->push_back(portal->id);

        const Vec3 * const verts = portal->verts;
        if (portal->vertexCount == 3)
        {
            addTriangle(verts[0], verts[1], verts[2]);
            (*outPortalVertsAdded) += 3;
        }
        else // Triangulate:
        {
            triangulateConvexPolygon(verts, portal->vertexCount, &triangles);
            assert((triangles.size() % 3) == 0);

            const int triangleCount = triangles.size() / 3;
            for (int t = 0; t < triangleCount; ++t)
            {
                addTriangle(triangles[(t * 3) + 0], triangles[(t * 3) + 1], triangles[(t * 3) + 2]);
                (*outPortalVertsAdded) += 3;
            }
        }

        nextPortalColor = (nextPortalColor + 1) % globalColorTableSize;
    }

    addDebugPortalsRecursive(world, node->frontNode, outPortalVertsAdded, outPortalIdsAdded);
    addDebugPortalsRecursive(world, node->backNode,  outPortalVertsAdded, outPortalIdsAdded);
}

void addDebugPortals(RenderData * world)
{
    assert(world != nullptr);

    int portalVertsAdded = 0;
    std::vector<int> portalIdsAdded;

    world->debugFirstPortalVert = world->getVertexCount();
    addDebugPortalsRecursive(world, world->bspRoot, &portalVertsAdded, &portalIdsAdded);
    world->debugPortalsVertCount = portalVertsAdded;
}

void renderDebugPortals(RenderData * world, const Mat4 & mvpMatrix)
{
    assert(world != nullptr);
    if (!g_bRenderDebugPortals || world->debugPortalsVertCount <= 0)
    {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    world->debugTexture.bind();
    world->debugPortalsShader.bind();

    world->debugPortalsShader.setUniform1i(world->debugPortalsBaseTextureLocation, 0);
    world->debugPortalsShader.setUniformMat4(world->debugPortalsMvpMatrixLocation, mvpMatrix);

    // All debug planes rendered, regardless of being in view.
    world->vertexArray.drawUnindexed(GL_TRIANGLES, world->debugFirstPortalVert, world->debugPortalsVertCount);

    glDisable(GL_BLEND);
}

// ========================================================
// Binary Space Partitioning setup:
// ========================================================

void splitTriangle(RenderData * world, const Polygon & triangleToSplit, const Plane & partition,
                   PolygonList * outFrontList, PolygonList * outBackList)
{
    assert(triangleToSplit.isTriangle());

    Vec3  pointA;
    Vec3  pointB;
    float uvA[2];
    float uvB[2];
    PlaneClassification sideA;
    PlaneClassification sideB;

    int frontCount = 0;
    int backCount  = 0;

    Vec3 frontPoints[4];
    Vec3 backPoints[4];
    Vec3 intersectionPoint;

    float frontUVs[4][2];
    float backUVs[4][2];
    float intersectionUV[2];

    const int vertexCount = triangleToSplit.vertexCount;
    const GLDrawVertex * dv = &world->vertexes[triangleToSplit.firstVertex + (vertexCount - 1)];

    uvA[0] = dv->u;
    uvA[1] = dv->v;
    pointA = Vec3{ dv->px, dv->py, dv->pz };
    sideA  = partition.classifyPoint(pointA);

    for (int v = -1; ++v < vertexCount;)
    {
        dv = &world->vertexes[triangleToSplit.firstVertex + v];

        uvB[0] = dv->u;
        uvB[1] = dv->v;
        pointB = Vec3{ dv->px, dv->py, dv->pz };
        sideB  = partition.classifyPoint(pointB);

        if (sideB == FrontSide)
        {
            if (sideA == BackSide)
            {
                intersectionPoint = calcEdgeIntersection(pointA, pointB, partition);
                calcTexCoords(pointA, uvA, pointB, uvB, intersectionPoint, intersectionUV);

                frontUVs[frontCount][0]   = intersectionUV[0];
                frontUVs[frontCount][1]   = intersectionUV[1];
                frontPoints[frontCount++] = intersectionPoint;

                backUVs[backCount][0]     = intersectionUV[0];
                backUVs[backCount][1]     = intersectionUV[1];
                backPoints[backCount++]   = intersectionPoint;
            }

            frontUVs[frontCount][0]   = uvB[0];
            frontUVs[frontCount][1]   = uvB[1];
            frontPoints[frontCount++] = pointB;
        }
        else if (sideB == BackSide)
        {
            if (sideA == FrontSide)
            {
                intersectionPoint = calcEdgeIntersection(pointA, pointB, partition);
                calcTexCoords(pointA, uvA, pointB, uvB, intersectionPoint, intersectionUV);

                frontUVs[frontCount][0]   = intersectionUV[0];
                frontUVs[frontCount][1]   = intersectionUV[1];
                frontPoints[frontCount++] = intersectionPoint;

                backUVs[backCount][0]     = intersectionUV[0];
                backUVs[backCount][1]     = intersectionUV[1];
                backPoints[backCount++]   = intersectionPoint;
            }

            backUVs[backCount][0]   = uvB[0];
            backUVs[backCount][1]   = uvB[1];
            backPoints[backCount++] = pointB;
        }
        else
        {
            assert(sideB == OnPlane);

            frontUVs[frontCount][0]   = uvB[0];
            frontUVs[frontCount][1]   = uvB[1];
            frontPoints[frontCount++] = pointB;

            backUVs[backCount][0]     = uvB[0];
            backUVs[backCount][1]     = uvB[1];
            backPoints[backCount++]   = pointB;
        }

        uvA[0] = uvB[0];
        uvA[1] = uvB[1];
        pointA = pointB;
        sideA  = sideB;
    }

    assert(frontCount <= arrayLength(frontPoints));
    assert(backCount  <= arrayLength(backPoints));

    auto makeTriangle = [world](const Vec3 & a, const float aUV[2],
                                const Vec3 & b, const float bUV[2],
                                const Vec3 & c, const float cUV[2])
    {
        Polygon * triangle    = world->allocPolygon();
        triangle->firstVertex = world->getVertexCount();
        triangle->vertexCount = 3;
        triangle->plane.fromPoints(a, b, c);

        const Vec3 n = triangle->plane.normal;
        GLDrawVertex vert{ 0.0f,0.0f,0.0f, n[0],n[1],n[2], 0.0f,0.0f,0.0f,1.0f,
                           0.0f,0.0f, 0.0f,0.0f,0.0f, 0.0f,0.0f,0.0f };

        vert.px = a[0];
        vert.py = a[1];
        vert.pz = a[2];
        vert.r  = 1.0f;
        vert.g  = 0.0f;
        vert.b  = 0.0f;
        vert.u  = aUV[0];
        vert.v  = aUV[1];
        world->addVertex(vert);

        vert.px = b[0];
        vert.py = b[1];
        vert.pz = b[2];
        vert.r  = 0.0f;
        vert.g  = 1.0f;
        vert.b  = 0.0f;
        vert.u  = bUV[0];
        vert.v  = bUV[1];
        world->addVertex(vert);

        vert.px = c[0];
        vert.py = c[1];
        vert.pz = c[2];
        vert.r  = 0.0f;
        vert.g  = 0.0f;
        vert.b  = 1.0f;
        vert.u  = cUV[0];
        vert.v  = cUV[1];
        world->addVertex(vert);

        return triangle;
    };

    if (frontCount == 4) // Two triangles are in the front, one behind
    {
        assert(backCount == 3);

        outFrontList->push_back(makeTriangle(frontPoints[0], frontUVs[0],
                                             frontPoints[1], frontUVs[1],
                                             frontPoints[2], frontUVs[2]));

        outFrontList->push_back(makeTriangle(frontPoints[0], frontUVs[0],
                                             frontPoints[2], frontUVs[2],
                                             frontPoints[3], frontUVs[3]));

        outBackList->push_back(makeTriangle(backPoints[0], backUVs[0],
                                            backPoints[1], backUVs[1],
                                            backPoints[2], backUVs[2]));
    }
    else if (backCount == 4) // One triangle is in the front, two behind
    {
        assert(frontCount == 3);

        outFrontList->push_back(makeTriangle(frontPoints[0], frontUVs[0],
                                             frontPoints[1], frontUVs[1],
                                             frontPoints[2], frontUVs[2]));

        outBackList->push_back(makeTriangle(backPoints[0], backUVs[0],
                                            backPoints[1], backUVs[1],
                                            backPoints[2], backUVs[2]));

        outBackList->push_back(makeTriangle(backPoints[0], backUVs[0],
                                            backPoints[2], backUVs[2],
                                            backPoints[3], backUVs[3]));
    }
    else if (frontCount == 3 && backCount == 3) // Plane bisects the triangle
    {
        outFrontList->push_back(makeTriangle(frontPoints[0], frontUVs[0],
                                             frontPoints[1], frontUVs[1],
                                             frontPoints[2], frontUVs[2]));

        outBackList->push_back(makeTriangle(backPoints[0], backUVs[0],
                                            backPoints[1], backUVs[1],
                                            backPoints[2], backUVs[2]));
    }
    else // Triangle must be totally in front of or behind the plane, so there's nothing to split.
    {
        if (backCount > frontCount)
        {
            assert(backCount == 3 && frontCount == 0);
            outBackList->push_back(makeTriangle(backPoints[0], backUVs[0],
                                                backPoints[1], backUVs[1],
                                                backPoints[2], backUVs[2]));
        }
        else
        {
            assert(frontCount == 3 && backCount == 0);
            outFrontList->push_back(makeTriangle(frontPoints[0], frontUVs[0],
                                                 frontPoints[1], frontUVs[1],
                                                 frontPoints[2], frontUVs[2]));
        }
    }
}

const Polygon * selectPartitionfromList(const RenderData & world, const PolygonList & polygonList)
{
    int notSplitCount = 0;
    int absDifference = 1000000000;
    const Polygon * bestPartition = nullptr;

    const Polygon * potentialPartition = polygonList.first();
    for (int i = polygonList.size(); i--; potentialPartition = potentialPartition->next)
    {
        int frontCount = 0;
        int backCount  = 0;
        const Plane partition = potentialPartition->plane;

        const Polygon * polyToClassify = polygonList.first();
        for (int j = polygonList.size(); j--; polyToClassify = polyToClassify->next)
        {
            PlaneClassification classifications[3]; // One for each vertex of the triangle
            partition.classifyTriangleVerts(*polyToClassify, world.vertexes, classifications);

            for (int c = 0; c < 3; ++c)
            {
                switch (classifications[c])
                {
                case BackSide :
                    ++backCount;
                    break;

                case FrontSide :
                    ++frontCount;
                    break;

                case OnPlane :
                    // Check the direction of the normal vector:
                    if (partition.classifyPoint(polyToClassify->plane.normal) == BackSide)
                    {
                        ++backCount;
                    }
                    else
                    {
                        ++frontCount;
                    }
                    break;

                default :
                    assert(false);
                    break;
                } // switch (classifications[c])
            }
        }

        if (std::abs(frontCount - backCount) < absDifference)
        {
            absDifference = std::abs(frontCount - backCount);
            bestPartition = potentialPartition;
        }
        if (frontCount == 0 || backCount == 0)
        {
            ++notSplitCount;
        }
    }

    return (notSplitCount == polygonList.size() ? nullptr : bestPartition);
}

void buildBspTreeRecursive(RenderData * world, BspNode * node)
{
    assert(world != nullptr);
    assert(node  != nullptr);

    const Polygon * partitionPolygon = selectPartitionfromList(*world, node->polygons);
    if (partitionPolygon == nullptr)
    {
        node->isLeaf = true;
        node->id     = ++world->bspLeafCount;
        return;
    }

    node->isLeaf    = false;
    node->id        = ++world->bspPartitionCount;
    node->partition = partitionPolygon->plane;
    node->backNode  = world->allocBspNode();
    node->frontNode = world->allocBspNode();

    // Classify each polygon in the current node with respect to the partitioning plane.
    Polygon * poly;
    while ((poly = node->polygons.pop_front()) != nullptr)
    {
        const auto side = node->partition.classifyPolygon(*poly, world->vertexes);
        switch (side)
        {
        case OnPlane :
            // Check the direction of the normal vector:
            if (node->partition.classifyPoint(poly->plane.normal) == BackSide)
            {
                node->backNode->polygons.push_back(poly);
            }
            else
            {
                node->frontNode->polygons.push_back(poly);
            }
            ++g_nPolysOnPlane;
            break;

        case BackSide :
            node->backNode->polygons.push_back(poly);
            ++g_nPolysBackSide;
            break;

        case FrontSide :
            node->frontNode->polygons.push_back(poly);
            ++g_nPolysFrontSide;
            break;

        case Spanning :
            // Break the triangle into up to 3 new triangles:
            splitTriangle(world, *poly, node->partition, &node->frontNode->polygons, &node->backNode->polygons);
            ++g_nPolysSpanning;
            break;
        } // switch (side)
    }

    assert(node->polygons.empty());
    buildBspTreeRecursive(world, node->frontNode);
    buildBspTreeRecursive(world, node->backNode);
}

// ========================================================
// PVS computation and BSP Tree rendering:
// ========================================================

int countVisibleLeaves(const RenderData & world)
{
    int visibleCount = 0;
    for (const BspNode * node : world.bspLeafNodes)
    {
        if (node->visFrame == g_nFrameNumber)
        {
            ++visibleCount;
        }
    }
    return visibleCount;
}

BspNode * findLeafRecursive(const Vec3 & referencePosition, BspNode * node)
{
    assert(node != nullptr);

    if (node->isLeaf)
    {
        return node;
    }

    const auto side = node->partition.classifyPoint(referencePosition);

    if (side == FrontSide || side == OnPlane)
    {
        return findLeafRecursive(referencePosition, node->frontNode);
    }
    else
    {
        return findLeafRecursive(referencePosition, node->backNode);
    }
}

void findVisibleLeavesRecursive(const Vec3 & eye, const int parentId, const Portal * currentPortal, const Portal * inputPortal)
{
    assert(currentPortal != nullptr);
    assert(inputPortal   != nullptr);

    // Create the planes from currentPortal:
    Plane planes[Portal::MaxVerts];
    const Vec3 pointOnPlane = eye;
    const int numPlanes = currentPortal->vertexCount;

    assert(numPlanes <= arrayLength(planes));
    for (int p = 0; p < numPlanes; ++p)
    {
        const int Nminus1 = (p == 0 ? numPlanes - 1 : p - 1);

        // Get the normal from edges:
        const Vec3 edge1 = currentPortal->verts[p] - eye;
        const Vec3 edge2 = currentPortal->verts[Nminus1] - eye;

        planes[p].normal = normalize(cross(edge1, edge2));
        planes[p].recalculateDistance(pointOnPlane);
    }

    assert(inputPortal->backLeaf != nullptr);
    BspNode * currentLeaf = inputPortal->backLeaf;
    currentLeaf->visFrame = g_nFrameNumber; // Mark visible.

    // Loop through the portals of this leaf node:
    const Portal * portal = currentLeaf->portals.first();
    for (int i = currentLeaf->portals.size(); i--; portal = portal->next)
    {
        currentPortal = portal;

        // If the back leaf isn't the parent:
        if (currentPortal->backLeaf->id != parentId)
        {
            Portal frontPortal[Portal::MaxVerts];
            int frontCount = 0;

            // Loop through the frustum planes:
            for (int planeNum = 0; planeNum < numPlanes; ++planeNum)
            {
                Portal backPortal;
                PlaneClassification side;
                const bool portalWasSplit = splitPortal(currentPortal, planes[planeNum], &frontPortal[planeNum], &backPortal, &side);

                if (portalWasSplit)
                {
                    currentPortal = &frontPortal[planeNum];
                }

                if (side != BackSide)
                {
                    ++frontCount;
                }
            }

            // If the portal was split by or infront of all the frustum planes:
            if (frontCount == numPlanes)
            {
                findVisibleLeavesRecursive(eye, inputPortal->backLeaf->id, currentPortal, portal);
            }
        }
    }
}

void computePotentiallyVisibleSet(const Vec3 & eye, const Frustum & frustum, BspNode * currentLeaf)
{
    assert(currentLeaf != nullptr);

    // Marked it to draw this frame.
    currentLeaf->visFrame = g_nFrameNumber;

    const Portal * portal = currentLeaf->portals.first();
    for (int i = currentLeaf->portals.size(); i--; portal = portal->next)
    {
        Plane  partition;
        Portal frontPortal[6];

        const Portal * currentPortal = portal;
        int frontCount = 0;

        // Loop through the frustum planes:
        for (int planeNum = 0; planeNum < 6; ++planeNum)
        {
            if (planeNum == 5)
            {
                // Skip the near frustum plane test.
                continue;
            }

            partition.normal = Vec3{
                frustum.p[planeNum][Frustum::A],
                frustum.p[planeNum][Frustum::B],
                frustum.p[planeNum][Frustum::C]
            };
            partition.distance = frustum.p[planeNum][Frustum::D];

            Portal backPortal;
            PlaneClassification side;
            const bool portalWasSplit = splitPortal(currentPortal, partition, &frontPortal[planeNum], &backPortal, &side);

            if (portalWasSplit)
            {
                currentPortal = &frontPortal[planeNum];
            }

            if (side != BackSide)
            {
                ++frontCount;
            }
        }

        // If the portal was split by or infront of the frustum planes:
        if (frontCount == 5)
        {
            findVisibleLeavesRecursive(eye, currentLeaf->id, currentPortal, portal);
        }
    }
}

void renderPolygonList(const RenderData & world, const PolygonList & polygonList)
{
    const Polygon * poly = polygonList.first();
    for (int count = polygonList.size(); count--; poly = poly->next)
    {
        assert(poly->isTriangle());

        // TODO Should probably look into batching the draw calls into an indirect-draw command buffer.
        // Back in the days of fixed GL, drawing individual polygons was not a problem, but nowadays this
        // setup probably outweighs the gains from the space partitioning...
        world.vertexArray.drawUnindexed(GL_TRIANGLES, poly->firstVertex, poly->vertexCount);
        ++g_nPolysRendered;
    }

    ++g_nPolyListsRendered;
}

void renderBspTreeRecursive(const RenderData & world, const Vec3 & eyePosition, const BspNode * node)
{
    assert(node != nullptr);

    // Find the leaf nodes containing geometry:
    if (!node->isLeaf)
    {
        const auto side = node->partition.classifyPoint(eyePosition);
        switch (side)
        {
        case FrontSide :
            renderBspTreeRecursive(world, eyePosition, node->backNode);
            renderBspTreeRecursive(world, eyePosition, node->frontNode);
            break;

        default :
            renderBspTreeRecursive(world, eyePosition, node->frontNode);
            renderBspTreeRecursive(world, eyePosition, node->backNode);
            break;
        } // switch (side)
    }
    else
    {
        if (node->visFrame == g_nFrameNumber)
        {
            // Visible leaf node, has geometry to draw:
            renderPolygonList(world, node->polygons);
        }
    }
}

void render(RenderData * world, const Vec3 & eyePosition, const Mat4 & viewMatrix, const Mat4 & mvpMatrix)
{
    assert(world != nullptr);

    if (g_bRenderWorldWrireframe && !g_bRenderWorldSolid)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    world->debugTexture.bind();
    world->mainShader.bind();

    world->mainShader.setUniform1i(world->mainRenderOutlineLocation, g_bRenderWorldWrireframe);
    world->mainShader.setUniform1i(world->mainBaseTextureLocation, 0);
    world->mainShader.setUniformMat4(world->mainMvpMatrixLocation, mvpMatrix);
    world->mainShader.setUniformMat4(world->mainModelViewMatrixLocation, viewMatrix);

    if (g_bBuildBspTree && g_bRenderUseBsp)
    {
        g_nPolyListsRendered = 0;
        g_nPolysRendered = 0;

        if (g_bRenderWithDepthTest)
        {
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }

        world->vertexArray.bindVA();
        renderBspTreeRecursive(*world, eyePosition, world->bspRoot);
        renderDebugPortals(world, mvpMatrix);
        world->vertexArray.bindNull();
    }
    else
    {
        g_nPolyListsRendered = 1;
        g_nPolysRendered = world->vertexArray.getVertexCount() / 3;

        world->vertexArray.bindVA();
        world->vertexArray.drawUnindexed(GL_TRIANGLES, 0,
                world->vertexArray.getVertexCount() - world->debugPortalsVertCount);
        world->vertexArray.bindNull();
    }

    if (g_bRenderWorldWrireframe && !g_bRenderWorldSolid)
    {
        glDisable(GL_BLEND);
    }

    ++g_nFrameNumber;
}

// ========================================================
// World loading / geometry setup:
// ========================================================

void createFromPolygons(RenderData * world, const Triangle * worldPolys, const int worldPolyCount)
{
    assert(world      != nullptr);
    assert(worldPolys != nullptr);

    // For the outline drawing using barycentric coords, as described in:
    // http://codeflow.org/entries/2012/aug/02/easy-wireframe-display-with-barycentric-coordinates/
    const float bc[3][3]{
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }
    };

    // Polygons are actually triangles for our demo.
    world->preAllocVertexes(worldPolyCount * 3);
    world->bspRoot = world->allocBspNode();

    for (int p = 0; p < worldPolyCount; ++p)
    {
        // Add a polygon:
        Polygon * newPoly    = world->allocPolygon();
        newPoly->firstVertex = world->getVertexCount();
        newPoly->vertexCount = 3;
        world->bspRoot->polygons.push_back(newPoly);

        // Add the vertexes:
        GLDrawVertex verts[3];
        const Triangle & poly = worldPolys[p];
        for (int v = 0; v < 3; ++v)
        {
            verts[v].px = poly.verts[v][0];
            verts[v].py = poly.verts[v][1];
            verts[v].pz = poly.verts[v][2];
            verts[v].r  = bc[v][0];
            verts[v].g  = bc[v][1];
            verts[v].b  = bc[v][2];
            verts[v].a  = 1.0f;
            verts[v].u  = poly.verts[v][3];
            verts[v].v  = poly.verts[v][4];

            // Unused for now.
            verts[v].tx = 0.0f;
            verts[v].ty = 0.0f;
            verts[v].tz = 0.0f;
            verts[v].bx = 0.0f;
            verts[v].by = 0.0f;
            verts[v].bz = 0.0f;
        }

        newPoly->plane.fromPoints(
                Vec3{ verts[0].px, verts[0].py, verts[0].pz },
                Vec3{ verts[1].px, verts[1].py, verts[1].pz },
                Vec3{ verts[2].px, verts[2].py, verts[2].pz });

        // The plane normal is used as the triangle normal for lighting.
        const Vec3 n = newPoly->plane.normal;
        for (int v = 0; v < 3; ++v)
        {
            verts[v].nx = n[0];
            verts[v].ny = n[1];
            verts[v].nz = n[2];
            world->addVertex(verts[v]);
        }
    }

    world->computeBounds();

    // Reset debug stats counters:
    g_nPolysOnPlane   = 0;
    g_nPolysBackSide  = 0;
    g_nPolysFrontSide = 0;
    g_nPolysSpanning  = 0;

    // Construct the BSP three:
    if (g_bBuildBspTree)
    {
        buildBspTreeRecursive(world, world->bspRoot);
        buildPortals(world);
        addDebugPortals(world);
    }

    // Send the GL render data to the GPU.
    world->submitGLVertexArray();
    world->loadTextures();
    world->loadShaders();
}

bool createFromDatafile(RenderData * world, const char * const filename, const float scale)
{
    FILE * fileIn = std::fopen(filename, "rt");
    if (fileIn == nullptr)
    {
        return false;
    }

    int polyCount;
    if (std::fscanf(fileIn, "%i", &polyCount) != 1 || polyCount <= 0)
    {
        std::fclose(fileIn);
        return false;
    }

    Triangle poly;
    int polysRead;
    std::vector<Triangle> worldPolys;
    worldPolys.reserve(polyCount);

    for (polysRead = 0; polysRead < polyCount && !std::feof(fileIn); ++polysRead)
    {
        // Each vertex is composed of xyz uv
        for (int v = 0; v < 3; ++v)
        {
            for (int e = 0; e < 5; ++e)
            {
                if (std::fscanf(fileIn, "%f", &poly.verts[v][e]) != 1)
                {
                    std::fclose(fileIn);
                    return false;
                }
            }

            // Adjust the vertex scale:
            poly.verts[v][0] *= scale;
            poly.verts[v][1] *= scale;
            poly.verts[v][2] *= scale;
        }
        worldPolys.push_back(poly);
    }
    std::fclose(fileIn);

    if (polysRead != polyCount) // Check for a truncated file
    {
        return false;
    }
    else
    {
        createFromPolygons(world, worldPolys.data(), worldPolys.size());
        return true;
    }
}

} // namespace World {}

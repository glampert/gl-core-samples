
// ================================================================================================
// -*- C++ -*-
// File: world_bsp.cpp
// Author: Guilherme R. Lampert
// Created on: 04/07/16
//
// Brief:
//  Culling, scene management and rendering of world geometry using Quake-style
//  Binary Space Partitioning (BSP), Portals and the Potentially Visible Set (PVS).
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#include "framework/gl_utils.hpp"
#include "framework/camera.hpp"
#include "framework/world_rendering.hpp"

#include <cstdarg>
#include <cstdio>

// App constants:
constexpr int initialWinWidth  = 1024;
constexpr int initialWinHeight = 768;
constexpr float defaultClearColor[] { 0.0f, 0.0f, 0.0f, 1.0f };
const std::string baseWindowTitle   { "World BSP demo" };

// ========================================================
// class WorldBspApp:
// ========================================================

class WorldBspApp final
    : public GLFWApp
{
private:

    //
    // World rendering data:
    //
    World::RenderData world{ *this };
    GLBatchLineRenderer lineRenderer{ *this, 64 };

    int currentWorldMap = 0;
    const char * worldMapNames[2]{ "assets/maps/sample1.txt", "assets/maps/sample2.txt" };

    Frustum frustum{};
    Camera camera{ initialWinWidth, initialWinHeight, 60.0f, 0.5f, 1000.0f };

    //
    // Debug screen text:
    //
    static constexpr float scrTextStartX  = 10.0f;
    static constexpr float scrTextStartY  = 10.0f;
    static constexpr float scrTextScaling = 0.65f;
    const Vec4 scrTextColor{ 0.0f, 1.0f, 0.0f, 1.0f };

    float scrTextX = scrTextStartX;
    float scrTextY = scrTextStartY;
    GLBatchTextRenderer textRenderer{ *this, 128 };

    //
    // Current mouse and keyboard state:
    //
    static constexpr int maxMouseDelta = 10;
    struct {
        int  deltaX   = 0;
        int  deltaY   = 0;
        int  lastPosX = 0;
        int  lastPosY = 0;
        bool leftButtonDown  = false;
        bool rightButtonDown = false;
    } mouse;
    struct {
        bool wDown = false;
        bool sDown = false;
        bool aDown = false;
        bool dDown = false;
    } keys;

public:

    WorldBspApp();
    void onInit() override;
    void onFrameRender(std::int64_t currentTimeMillis, std::int64_t elapsedTimeMillis) override;
    void onMouseButton(MouseButton button, bool pressed) override;
    void onMouseMotion(int x, int y) override;
    void onMouseScroll(double xOffset, double yOffset) override;
    void onKey(int key, int action, int mods) override;
    void onKeyChar(unsigned int chr) override;
    void scrPrintF(const char * format, ...) ATTR_PRINTF_FUNC(2, 3);
};

// ========================================================
// WorldBspApp implementation:
// ========================================================

WorldBspApp::WorldBspApp()
    : GLFWApp(initialWinWidth, initialWinHeight, defaultClearColor, baseWindowTitle)
{
    printF("---- WorldBSP starting up... ----");
}

void WorldBspApp::onInit()
{
    // Disable so we can look at the world map from outside.
    glDisable(GL_CULL_FACE);

    if (!World::createFromDatafile(&world, worldMapNames[currentWorldMap]))
    {
        errorF("Unable to load world geometry from file \"%s\"!", worldMapNames[currentWorldMap]);
    }
    else
    {
        printF("World geometry loaded and BSP Tree built.");
        setWindowTitle(baseWindowTitle + " => " + worldMapNames[currentWorldMap]);

        lineRenderer.addBoundingBox(Point3{ world.bounds.mins },
                                    Point3{ world.bounds.maxs },
                                    Vec4{ 1.0f, 1.0f, 0.0f, 1.0f });
    }
}

void WorldBspApp::onFrameRender(std::int64_t /* currentTimeMillis */, const std::int64_t elapsedTimeMillis)
{
    const double deltaSeconds = millisToSeconds(elapsedTimeMillis);

    camera.checkKeyboardMovement(keys.wDown, keys.sDown, keys.aDown, keys.dDown, deltaSeconds);
    if (mouse.leftButtonDown)
    {
        camera.checkMouseRotation(mouse.deltaX, mouse.deltaY, deltaSeconds);
    }
    camera.updateMatrices();

    World::BspNode * currentLeaf = nullptr;
    if (World::g_bBuildBspTree && World::g_bRenderUseBsp)
    {
        frustum.computeClippingPlanes(camera.viewMatrix, camera.projMatrix);
        currentLeaf = World::findLeafRecursive(camera.eye, world.bspRoot);
        World::computePotentiallyVisibleSet(camera.eye, frustum, currentLeaf);
    }

    const int numVisLeaves = World::countVisibleLeaves(world);
    World::render(&world, camera.eye, camera.viewMatrix, camera.vpMatrix);

    lineRenderer.setLinesMvpMatrix(camera.vpMatrix);
    lineRenderer.drawLines();

    scrPrintF("BSP tree built..........: %s\n", (World::g_bBuildBspTree ? "yes" : "no"));
    scrPrintF("BSP tree rendering......: %s\n", (World::g_bRenderUseBsp ? "yes" : "no"));
    scrPrintF("GL Depth test enabled...: %s\n", (World::g_bRenderWithDepthTest ? "yes" : "no"));
    scrPrintF("Polygons <OnPlane>......: %i\n", World::g_nPolysOnPlane);
    scrPrintF("Polygons <FrontSide>....: %i\n", World::g_nPolysFrontSide);
    scrPrintF("Polygons <BackSide>.....: %i\n", World::g_nPolysBackSide);
    scrPrintF("Polygons <Spanning>.....: %i\n", World::g_nPolysSpanning);
    scrPrintF("Polygons rendered.......: %i\n", World::g_nPolysRendered);
    scrPrintF("Polygon lists rendered..: %i\n", World::g_nPolyListsRendered);
    scrPrintF("Num portals.............: %i\n", world.bspPortalCount);
    scrPrintF("Num BSP leaves..........: %i\n", world.bspLeafCount);
    scrPrintF("Visible BSP leaves......: %i\n", numVisLeaves);
    scrPrintF("Current BSP leaf........: %i\n", (currentLeaf != nullptr ? currentLeaf->id : -1));

    textRenderer.drawText(getWindowWidth(), getWindowHeight());
    textRenderer.clear();
    scrTextY = scrTextStartY;
}

void WorldBspApp::scrPrintF(const char * format, ...)
{
    va_list vaArgs;
    char buffer[4096];

    va_start(vaArgs, format);
    const int result = std::vsnprintf(buffer, arrayLength(buffer), format, vaArgs);
    va_end(vaArgs);

    if (result > 0 && result < arrayLength(buffer))
    {
        textRenderer.addText(scrTextX, scrTextY, scrTextScaling, scrTextColor, buffer);
        scrTextY += textRenderer.getCharHeight() * scrTextScaling;
    }
}

void WorldBspApp::onMouseButton(const MouseButton button, const bool pressed)
{
    if (button == MouseButton::Right)
    {
        mouse.rightButtonDown = pressed;
    }
    else if (button == MouseButton::Left)
    {
        mouse.leftButtonDown = pressed;
    }
}

void WorldBspApp::onMouseMotion(int x, int y)
{
    // Clamp to window bounds:
    if (x > getWindowWidth())  { x = getWindowWidth(); }
    else if (x < 0)            { x = 0; }
    if (y > getWindowHeight()) { y = getWindowHeight(); }
    else if (y < 0)            { y = 0; }

    mouse.deltaX = x - mouse.lastPosX;
    mouse.deltaY = y - mouse.lastPosY;
    mouse.lastPosX = x;
    mouse.lastPosY = y;

    // Clamp between -/+ max delta:
    if      (mouse.deltaX >  maxMouseDelta) { mouse.deltaX =  maxMouseDelta; }
    else if (mouse.deltaX < -maxMouseDelta) { mouse.deltaX = -maxMouseDelta; }
    if      (mouse.deltaY >  maxMouseDelta) { mouse.deltaY =  maxMouseDelta; }
    else if (mouse.deltaY < -maxMouseDelta) { mouse.deltaY = -maxMouseDelta; }
}

void WorldBspApp::onMouseScroll(double /* xOffset */, const double yOffset)
{
    float newFovY = camera.fovYDegrees;
    if (yOffset < 0.0)
    {
        newFovY += 0.2f;
    }
    else if (yOffset > 0.0f)
    {
        newFovY -= 0.2f;
    }

    newFovY = clamp(newFovY, 1.0f, 100.0f);
    camera.adjustFov(getWindowWidth(), getWindowHeight(), newFovY, 0.5f, 1000.0f);
}

void WorldBspApp::onKey(const int key, const int action, int /* mods */)
{
    if      (key == GLFW_KEY_A || key == GLFW_KEY_LEFT)  { keys.aDown = (action != GLFW_RELEASE); }
    else if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT) { keys.dDown = (action != GLFW_RELEASE); }
    else if (key == GLFW_KEY_W || key == GLFW_KEY_UP)    { keys.wDown = (action != GLFW_RELEASE); }
    else if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN)  { keys.sDown = (action != GLFW_RELEASE); }
}

void WorldBspApp::onKeyChar(const unsigned int chr)
{
    if (chr == 'n') // Cycle the available world maps
    {
        world.cleanup();
        currentWorldMap = (currentWorldMap + 1) % arrayLength(worldMapNames);

        if (!World::createFromDatafile(&world, worldMapNames[currentWorldMap]))
        {
            errorF("Unable to load world geometry from file \"%s\"!", worldMapNames[currentWorldMap]);
        }
        else
        {
            printF("World geometry loaded and BSP Tree built.");
            setWindowTitle(baseWindowTitle + " => " + worldMapNames[currentWorldMap]);

            lineRenderer.clear();
            lineRenderer.addBoundingBox(Point3{ world.bounds.mins },
                                        Point3{ world.bounds.maxs },
                                        Vec4{ 1.0f, 1.0f, 0.0f, 1.0f });
        }
    }
    else if (chr == 't')
    {
        World::g_bBuildBspTree = !World::g_bBuildBspTree;
    }
    else if (chr == 'b')
    {
        World::g_bRenderUseBsp = !World::g_bRenderUseBsp;
    }
    else if (chr == 'z')
    {
        World::g_bRenderWithDepthTest = !World::g_bRenderWithDepthTest;
    }
    else if (chr == 'p')
    {
        World::g_bRenderDebugPortals = !World::g_bRenderDebugPortals;
    }
    else if (chr == 'k')
    {
        World::g_bRenderWorldWrireframe = !World::g_bRenderWorldWrireframe;
    }
    else if (chr == 'l')
    {
        World::g_bRenderWorldSolid = !World::g_bRenderWorldSolid;
    }
}

// ========================================================
// AppFactory:
// ========================================================

GLFWApp::Ptr AppFactory::createGLFWAppInstance()
{
    return GLFWApp::Ptr{ new WorldBspApp() };
}

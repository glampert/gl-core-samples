
// ================================================================================================
// -*- C++ -*-
// File: doom3_models.cpp
// Author: Guilherme R. Lampert
// Created on: 06/02/16
// Brief: Animated Doom 3 models sample using GPU skinning.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#include "framework/gl_utils.hpp"
#include "framework/doom3md5.hpp"

// App constants:
constexpr int initialWinWidth  = 1024;
constexpr int initialWinHeight = 768;
constexpr float defaultClearColor[] { 0.1f, 0.1f, 0.1f, 1.0f     };
const std::string baseWindowTitle   { "DOOM 3 MD5 Models demo"   };
const std::string lightCookieFile   { "assets/cookie0"           };
const std::string floorTileFile     { "assets/floor_tile"        };
const std::string animBasePath      { "assets/hellknight/anims/" };

// ========================================================
// class Doom3ModelsApp:
// ========================================================

//
// User interaction keys:
//  [N] -> Cycles the model animation.
//  [H] -> Return the model to bind/home pose.
//  [P] -> Pause/resume the current animation.
//  [T] -> Toggle display of the tangent basis vectors (normal-mapping).
//  [S] -> Toggle display of the skeleton and joints as wireframe/points.
//  [R] -> Toggle auto rotation of the scene.
//  [F] -> Toggle the flashlight on/off.
//  [X] -> Toggle shadow rendering.
//
// Mouse buttons:
//  [RIGHT BTN]    -> Toggle the flashlight on/off.
//  [LEFT  BTN]    -> Click and hold to move the flashlight.
//  [WHEEL/SCROLL] -> Zoom in/out.
//
class Doom3ModelsApp final
    : public GLFWApp
{
private:

    // Set of animation for the hellknight model:
    const std::vector<std::string> animFiles
    {
        animBasePath + "idle.md5anim",
        animBasePath + "stand.md5anim",
        animBasePath + "attack1.md5anim",
        animBasePath + "attack2.md5anim",
        animBasePath + "range_attack.md5anim",
        animBasePath + "turret_attack.md5anim",
        animBasePath + "left_slash.md5anim",
        animBasePath + "roar.md5anim",
        animBasePath + "pain.md5anim",
        animBasePath + "chest_pain.md5anim",
        animBasePath + "head_pain.md5anim",
        animBasePath + "pain_luparm.md5anim",
        animBasePath + "pain_ruparm.md5anim",
        animBasePath + "walk.md5anim",
        animBasePath + "walk_left.md5anim",
        animBasePath + "ik_pose.md5anim",
        animBasePath + "initial.md5anim"
    };

    // Model and misc switches:
    DOOM3::AnimatedEntity entity       { *this, "assets/hellknight/hellknight.md5mesh", animFiles };
    int   currAnimNum                  { 0       };
    bool  pauseAnim                    { false   };
    bool  showSkeleton                 { false   };
    bool  showTangentBasis             { false   };
    bool  autoRotate                   { true    };
    bool  drawShadow                   { true    };
    bool  flashlightOn                 { false   };
    float modelZoom                    { -7.0f   };
    float modelRotationDegreesY        {  180.0f };

    // Floor plane (made of several small triangular tiles):
    GLVertexArray floorPlane           { *this };
    GLTexture floorBaseTexture         { *this };
    GLTexture floorNormalTexture       { *this };
    GLTexture floorSpecularTexture     { *this };

    // Camera matrices:
    Mat4 projMatrix                    { Mat4::identity() };
    Mat4 viewMatrix                    { Mat4::identity() };

    // Viewer won't move. We rotate the scene instead.
    const Point3 eyePosition           { 0.0f,  2.0f,  4.0f };
    const Point3 eyeLookAt             { 0.0f,  0.0f, -1.0f };

    // For debugging visualization of the skeleton and tangent-basis:
    GLBatchLineRenderer  lineRenderer  { *this, 1024 };
    GLBatchPointRenderer pointRenderer { *this, 128  };

    // Light sources: A flashlight and a fixed point light:
    GLTexture flashlightCookieTexture  { *this };
    DOOM3::PointLightSource pointLight { };
    DOOM3::FlashlightSource flashLight { };
    float flashlightDegreesRotationX   { 0.0f };
    float flashlightDegreesRotationY   { 0.0f };

    // Current mouse state:
    static constexpr int maxMouseDelta = 10;
    struct {
        int  deltaX   = 0;
        int  deltaY   = 0;
        int  lastPosX = 0;
        int  lastPosY = 0;
        bool leftButtonDown  = false;
        bool rightButtonDown = false;
    } mouse;

public:

    Doom3ModelsApp();
    void makeFloorPlane();
    void onInit() override;
    void onFrameRender(std::int64_t currentTimeMillis, std::int64_t elapsedTimeMillis) override;
    void onMouseButton(MouseButton button, bool pressed) override;
    void onMouseMotion(int x, int y) override;
    void onMouseScroll(double xOffset, double yOffset) override;
    void onKeyChar(unsigned int chr) override;
};

// ========================================================
// Doom3ModelsApp implementation:
// ========================================================

Doom3ModelsApp::Doom3ModelsApp()
    : GLFWApp(initialWinWidth, initialWinHeight, defaultClearColor, baseWindowTitle)
{
    printF("---- Doom3ModelsApp starting up... ----");
}

void Doom3ModelsApp::onInit()
{
    viewMatrix = Mat4::lookAt(eyePosition, eyeLookAt, Vec3::yAxis());
    projMatrix = Mat4::perspective(degToRad(60.0f), aspectRatio(initialWinWidth, initialWinHeight), 0.5f, 1000.0f);

    // Flashlight light cookie texture (@ TMU 3):
    flashlightCookieTexture.initFromFile(lightCookieFile + ".png", false, GLTexture::Filter::Linear, GLTexture::WrapMode::Clamp, true, 3);

    // Ground plane textures:
    floorBaseTexture.initFromFile(floorTileFile + ".tga",         false, GLTexture::Filter::LinearMipmaps, GLTexture::WrapMode::Clamp, true, 0);
    floorNormalTexture.initFromFile(floorTileFile + "_local.tga", false, GLTexture::Filter::LinearMipmaps, GLTexture::WrapMode::Clamp, true, 1);
    floorSpecularTexture.initFromFile(floorTileFile + "_s.tga",   false, GLTexture::Filter::LinearMipmaps, GLTexture::WrapMode::Clamp, true, 2);

    // Set up the floor geometry:
    makeFloorPlane();

    // Position lights at the eye:
    pointLight.positionWorldSpace     = eyePosition;
    flashLight.positionWorldSpace     = eyePosition;
    flashLight.lookAtWorldSpace       = eyeLookAt;
    flashLight.lightPerspectiveMatrix = Mat4::perspective(degToRad(45.0f), aspectRatio(800.0f, 600.0f), 0.5f, 500.0f);
    flashLight.lightCookieTexture     = &flashlightCookieTexture;
}

void Doom3ModelsApp::makeFloorPlane()
{
    std::vector<GLDrawVertex> floorVerts;
    GLDrawVertex vert;

    constexpr int FloorRows  = 10;
    constexpr int FloorCols  = 10;
    constexpr float CellSize = 5.0f;

    // These are common for every vertex.
    // Normal:     unit Y
    // Tangent:    unit X
    // Bi-Tangent: unit Z
    vert.r  = 1.0f;
    vert.g  = 1.0f;
    vert.b  = 1.0f;
    vert.a  = 1.0f;
    vert.tx = 1.0f;
    vert.ty = 0.0f;
    vert.tz = 0.0f;
    vert.nx = 0.0f;
    vert.ny = 1.0f;
    vert.nz = 0.0f;
    vert.bx = 0.0f;
    vert.by = 0.0f;
    vert.bz = 1.0f;

    // Height is fixed.
    vert.py = 0.0f;

    for (int col = -FloorCols; col <= FloorCols; ++col)
    {
        for (int row = -FloorRows; row <= FloorRows; ++row)
        {
            const float x = row * CellSize;
            const float y = col * CellSize;

            // First triangle:
            vert.px = x;
            vert.pz = y;
            vert.u  = 0.0f;
            vert.v  = 0.0f;
            floorVerts.push_back(vert);
            vert.px = x;
            vert.pz = y + CellSize;
            vert.u  = 0.0f;
            vert.v  = 1.0f;
            floorVerts.push_back(vert);
            vert.px = x + CellSize;
            vert.pz = y + CellSize;
            vert.u  = 1.0f;
            vert.v  = 1.0f;
            floorVerts.push_back(vert);

            // Second triangle:
            vert.px = x + CellSize;
            vert.pz = y + CellSize;
            vert.u  = 1.0f;
            vert.v  = 1.0f;
            floorVerts.push_back(vert);
            vert.px = x + CellSize;
            vert.pz = y;
            vert.u  = 1.0f;
            vert.v  = 0.0f;
            floorVerts.push_back(vert);
            vert.px = x;
            vert.pz = y;
            vert.u  = 0.0f;
            vert.v  = 0.0f;
            floorVerts.push_back(vert);
        }
    }

    floorPlane.initFromData(floorVerts.data(), floorVerts.size(), nullptr, 0,
                            GL_STATIC_DRAW, GLVertexLayout::Triangles);
}

void Doom3ModelsApp::onFrameRender(std::int64_t /* currentTimeMillis */, const std::int64_t elapsedTimeMillis)
{
    //
    // Common transform/light updates:
    //

    if (mouse.leftButtonDown && flashlightOn)
    {
        flashlightDegreesRotationX -= mouse.deltaY;
        flashlightDegreesRotationY -= mouse.deltaX;
        flashlightDegreesRotationX = clamp(flashlightDegreesRotationX, -60.0f, 60.0f);
        flashlightDegreesRotationY = clamp(flashlightDegreesRotationY, -60.0f, 60.0f);

        mouse.deltaX = 0;
        mouse.deltaY = 0;

        // Flashlight mouse rotation:
        const Mat4 rotMat = Mat4::rotationX(degToRad(flashlightDegreesRotationX)) *
                            Mat4::rotationY(degToRad(flashlightDegreesRotationY));

        flashLight.lookAtWorldSpace   = toPoint3(rotMat * eyePosition);
        flashLight.positionWorldSpace = toPoint3(rotMat * eyeLookAt);
    }

    const auto elapsedTimeSeconds = millisToSeconds(elapsedTimeMillis);
    if (autoRotate)
    {
        modelRotationDegreesY += elapsedTimeSeconds * 10.0f; // ~10 degrees per second
    }

    const Mat4 sceneTranslation      = Mat4::translation(Vec3{ 0.0f, -6.0f, modelZoom });
    const Mat4 sceneRotation         = Mat4::rotationY(degToRad(modelRotationDegreesY));
    const Mat4 modelToWorldMatrix    = sceneTranslation * sceneRotation;
    const Mat4 invModelToWorldMatrix = inverse(modelToWorldMatrix);

    const Point3 eyePosModelSpace = worldPointToModel(invModelToWorldMatrix, eyePosition);
    pointLight.positionModelSpace = worldPointToModel(invModelToWorldMatrix, pointLight.positionWorldSpace);

    if (flashlightOn)
    {
        flashLight.positionModelSpace = worldPointToModel(invModelToWorldMatrix, flashLight.positionWorldSpace);
        flashLight.computeProjectionMatrix(invModelToWorldMatrix);
    }

    const DOOM3::LightBase * lights[] = { &pointLight, (flashlightOn ? &flashLight : nullptr) };

    //
    // DOOM3 model drawing / anim update:
    //

    if (!pauseAnim)
    {
        entity.updateAnimation(elapsedTimeSeconds);
        entity.updateModelPose();
    }

    const Mat4 mvpMatrix = projMatrix * viewMatrix * modelToWorldMatrix; // In OGL layout p*v*m
    entity.drawWholeModel(GL_TRIANGLES, mvpMatrix, eyePosModelSpace, nullptr, lights, (flashlightOn ? 2 : 1));

    //
    // Floor plane drawing:
    //

    floorBaseTexture.bind();
    floorNormalTexture.bind();
    floorSpecularTexture.bind();

    floorPlane.bindVA();
    floorPlane.draw(GL_TRIANGLES);
    floorPlane.bindNull();

    //
    // Simple plane-projected shadow for the point light:
    //

    if (drawShadow)
    {
        const auto shadowLightPos = toPoint3(Mat4::rotationY(degToRad(-modelRotationDegreesY)) * pointLight.positionWorldSpace);
        const Mat4 shadowOffset   = Mat4::translation(Vec3{ 0.0f, 0.1f, 0.0f });
        const Mat4 shadowMat      = makeShadowMatrix(Vec4::yAxis(), Vec4{ Vec3{ shadowLightPos }, 0.0f });
        const Mat4 shadowMvp      = mvpMatrix * shadowOffset * shadowMat;

        entity.drawWholeModelShadow(shadowMvp, pointLight.positionModelSpace);
    }

    //
    // Debug drawing:
    //

    if (showSkeleton)
    {
        entity.addSkeletonWireFrame(&lineRenderer, &pointRenderer);

        glDisable(GL_DEPTH_TEST);
        lineRenderer.setLinesMvpMatrix(mvpMatrix);
        lineRenderer.drawLines();
        lineRenderer.clear();
        pointRenderer.setPointsMvpMatrix(mvpMatrix);
        pointRenderer.drawPoints();
        pointRenderer.clear();
        glEnable(GL_DEPTH_TEST);
    }

    if (showTangentBasis)
    {
        entity.addTangentBasis(&lineRenderer, &pointRenderer);

        lineRenderer.setLinesMvpMatrix(mvpMatrix);
        lineRenderer.drawLines();
        lineRenderer.clear();
        pointRenderer.setPointsMvpMatrix(mvpMatrix);
        pointRenderer.drawPoints();
        pointRenderer.clear();
    }
}

void Doom3ModelsApp::onMouseButton(const MouseButton button, const bool pressed)
{
    if (button == MouseButton::Right) // Toggle flashlight on/of
    {
        if (pressed)
        {
            flashlightOn = !flashlightOn;
            mouse.rightButtonDown = true;
        }
        else
        {
            mouse.rightButtonDown = false;
        }
    }
    else if (button == MouseButton::Left) // Enable moving the flashlight
    {
        mouse.leftButtonDown = pressed;
    }
}

void Doom3ModelsApp::onMouseMotion(int x, int y)
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

void Doom3ModelsApp::onMouseScroll(double /* xOffset */, const double yOffset)
{
    if (yOffset < 0.0)
    {
        modelZoom += 0.4f;
    }
    else if (yOffset > 0.0f)
    {
        modelZoom -= 0.4f;
    }
    modelZoom = clamp(modelZoom, -100.0f, 1.0f);
}

void Doom3ModelsApp::onKeyChar(const unsigned int chr)
{
    if (chr == 'n') // Cycle animations
    {
        const auto anim = entity.findAnimation(animFiles[currAnimNum]);
        if (anim == nullptr)
        {
            return;
        }

        setWindowTitle(baseWindowTitle + " => " + animFiles[currAnimNum]);
        printF("Switching to animation: %s", animFiles[currAnimNum].c_str());

        currAnimNum = (currAnimNum + 1) % animFiles.size();
        entity.setAnimation(anim);
    }
    else if (chr == 'h') // Back to bind/home pose
    {
        currAnimNum = 0;
        entity.setAnimation(nullptr);

        printF("Resetting to bind/home pose...");
        setWindowTitle(baseWindowTitle + " => bind pose");
    }
    else if (chr == 'p') // Pause/resume the current animation
    {
        pauseAnim = !pauseAnim;
    }
    else if (chr == 't') // Toggle tangent-basis display
    {
        showTangentBasis = !showTangentBasis;
    }
    else if (chr == 's') // Toggle skeleton/joint display
    {
        showSkeleton = !showSkeleton;
    }
    else if (chr == 'r') // Toggle auto-rotation (on by default)
    {
        autoRotate = !autoRotate;
    }
    else if (chr == 'f') // Toggle flashlight
    {
        flashlightOn = !flashlightOn;
    }
    else if (chr == 'x') // Toggle shadow rendering
    {
        drawShadow = !drawShadow;
    }
}

// ========================================================
// AppFactory:
// ========================================================

GLFWApp::Ptr AppFactory::createGLFWAppInstance()
{
    return GLFWApp::Ptr{ new Doom3ModelsApp() };
}

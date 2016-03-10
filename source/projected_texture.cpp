
// ================================================================================================
// -*- C++ -*-
// File: projected_texture.cpp
// Author: Guilherme R. Lampert
// Created on: 14/11/15
// Brief: OpenGL projective texturing tests.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#include "framework/gl_utils.hpp"

// App constants:
constexpr int numOfLights      = 2;
constexpr int initialWinWidth  = 800;
constexpr int initialWinHeight = 600;
constexpr float defaultClearColor[]{ 0.7f, 0.7f, 0.7f, 1.0f };

// ========================================================
// struct ProjectedSpotlight:
// ========================================================

struct ProjectedSpotlight
{
    GLTexture lightCookieTexture;
    GLShaderProg & shaderProg;

    GLint lightCookieTexLocation;
    GLint lightProjMatrixLocation;
    GLint lightPosLocation;

    float lightRotationDegreesY;
    float lightRotationDir; // -1 or +1

    // Matrices to view the scene from the light's perspective:
    Mat4 projMatrix;
    Mat4 viewMatrix;

    // World positions:
    Point3 lookAtWorldPosition;
    Point3 lightWorldPosition;

    // Positions after rotation:
    Point3 lookAt;
    Point3 eyePos;

    // Sets up a default spotlight.
    ProjectedSpotlight(GLFWApp & owner, GLShaderProg & shader,
                       const Point3 & initialPos, const Point3 & initialLookAt,
                       const std::string & lightCookieImage, int lightNum);

    // Called every frame after binding the light shader to update the uniform vars.
    void onFrameRender(const Mat4 & invModelToWorldMatrix);

    // Optional. Can be called every frame to make the light source rotate, for a more dynamic scene.
    void animate(double elapsedTimeSeconds);
};

ProjectedSpotlight::ProjectedSpotlight(GLFWApp & owner, GLShaderProg & shader,
                                       const Point3 & initialPos, const Point3 & initialLookAt,
                                       const std::string & lightCookieImage, const int lightNum)
    : lightCookieTexture{ owner }
    , shaderProg{ shader }
{
    lightCookieTexture.initFromFile(lightCookieImage,
                                    /* flipV = */ false,
                                    GLTexture::Filter::Linear,
                                    GLTexture::WrapMode::Clamp,
                                    /* mipmaps = */ true,
                                    /* texUnit = */ lightNum + 1); // tmu:0 is already taken by the base texture(s), so +1

    const auto lightNumStr  = std::to_string(lightNum);
    lightCookieTexLocation  = shaderProg.getUniformLocation("u_ProjectedTexture[" + lightNumStr + "]");
    lightProjMatrixLocation = shaderProg.getUniformLocation("u_LightProjectionMatrix[" + lightNumStr + "]");
    lightPosLocation        = shaderProg.getUniformLocation("u_LightPositionModelSpace[" + lightNumStr + "]");

    // Initial positions:
    lightWorldPosition  = initialPos;
    lookAtWorldPosition = initialLookAt;
    lookAt = lookAtWorldPosition;
    eyePos = lightWorldPosition;

    // A view of the scene from the light's perspective.
    // Tweaking the projection parameters will change the shape of the light cone.
    projMatrix = Mat4::perspective(degToRad(45.0f), aspectRatio(800.0f, 600.0f), 0.5f, 500.0f);

    // The view matrix has to be recomputed from the
    // model-space position of the light source every frame.
    viewMatrix = Mat4::identity();

    // A random initial angle for the light source.
    lightRotationDegreesY = randomFloat(0.0f, 20.0f);

    // One rotates left and the other right.
    lightRotationDir = (lightNum & 1) ? 1.0f : -1.0f;
}

void ProjectedSpotlight::onFrameRender(const Mat4 & invModelToWorldMatrix)
{
    // Shader expects all positions in model space.
    const auto lookAtModelPosition = worldPointToModel(invModelToWorldMatrix, lookAt);
    const auto lightModelPosition  = worldPointToModel(invModelToWorldMatrix, eyePos);

    // Fixed up vector, always the positive Y axis.
    viewMatrix = Mat4::lookAt(lightModelPosition, lookAtModelPosition, Vec3::yAxis());

    // Bias matrix is a constant.
    // It performs a linear transformation to go from the [–1, 1]
    // range to the [0, 1] range. Having the coordinates in the [0, 1]
    // range is necessary for the values to be used as texture coordinates.
    const auto biasMatrix = Mat4{ Vec4{ 0.5f,  0.0f,  0.0f,  0.0f },
                                  Vec4{ 0.0f, -0.5f,  0.0f,  0.0f },
                                  Vec4{ 0.5f,  0.5f,  1.0f,  0.0f },
                                  Vec4{ 0.0f,  0.0f,  0.0f,  1.0f } };

    // The combined light view, projection and bias matrices to generate the projected texture coordinates.
    const Mat4 lightProjectionMatrix = biasMatrix * projMatrix * viewMatrix;

    // Send the shader parameters.
    // This assumes the shader is already bound!
    shaderProg.setUniform1i(lightCookieTexLocation, lightCookieTexture.getTexUnit());
    shaderProg.setUniformPoint3(lightPosLocation, lightModelPosition);
    shaderProg.setUniformMat4(lightProjMatrixLocation, lightProjectionMatrix);

    // Initialized with tmu 1 or 2
    lightCookieTexture.bind();
}

void ProjectedSpotlight::animate(const double elapsedTimeSeconds)
{
    lightRotationDegreesY += elapsedTimeSeconds * 20.0f; // 20 degrees per sec.

    // Apply the rotation:
    const Mat4 rotationMatrix = Mat4::rotationY(degToRad(lightRotationDegreesY * lightRotationDir));
    lookAt = toPoint3(rotationMatrix * lookAtWorldPosition);
    eyePos = toPoint3(rotationMatrix * lightWorldPosition);
}

// ========================================================
// class ProjTexApp:
// ========================================================

class ProjTexApp final
    : public GLFWApp
{
    // Our "fake" spotlights in the scene:
    std::unique_ptr<ProjectedSpotlight> spotlights[numOfLights];

    // A rotating teapot:
    GLVertexArray teapotObject   { *this };
    GLTexture teapotTexture      { *this };
    float teaporRotationDegreesY { 0.0f  };

    // And a ground plane (made with a flat box):
    GLVertexArray groundObject   { *this };
    GLTexture groundTexture      { *this };

    // The projective texture shader:
    GLShaderProg shaderProg      { *this };
    GLint colorTextureLocation   { -1 };
    GLint mvpMatrixLocation      { -1 };

    // Camera matrices (fixed):
    Mat4 projMatrix              { Mat4::identity() };
    Mat4 viewMatrix              { Mat4::identity() };

public:

    ProjTexApp();
    void drawTeapot(double elapsedTimeSeconds);
    void drawGroundPlane();

    void onInit() override;
    void onFrameRender(std::int64_t currentTimeMillis, std::int64_t elapsedTimeMillis) override;
};

ProjTexApp::ProjTexApp()
    : GLFWApp(initialWinWidth, initialWinHeight, defaultClearColor, "OpenGL Projective Texture demo")
{
    printF("---- ProjTexApp starting up... ----");
}

void ProjTexApp::onInit()
{
    viewMatrix = Mat4::lookAt(Point3{ 0.0f, 3.5f, 4.0f }, Point3{ 0.0f, 0.0f, -1.0f }, Vec3::yAxis());
    projMatrix = Mat4::perspective(degToRad(60.0f), aspectRatio(initialWinWidth, initialWinHeight), 0.5f, 1000.0f);

    // Shader program that performs the texture projection logic:
    shaderProg.initFromFiles("source/shaders/projtex.vert", "source/shaders/projtex.frag");
    mvpMatrixLocation    = shaderProg.getUniformLocation("u_MvpMatrix");
    colorTextureLocation = shaderProg.getUniformLocation("u_ColorTexture");

    //
    // Blue/white ground plane:
    //
    const float texColorsGround[][4]{
        { 0.0f, 0.0f, 0.9f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f }
    };
    groundTexture.initWithCheckerPattern(8, texColorsGround, GLTexture::Filter::Nearest, 0);
    groundObject.initWithBoxMesh(GL_STATIC_DRAW, 50.0f, 0.1f, 50.0f, nullptr);

    //
    // Red/green teapot at the center:
    //
    const float texColorsTeapot[][4]{
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f }
    };
    teapotTexture.initWithCheckerPattern(16, texColorsTeapot, GLTexture::Filter::Nearest, 0);
    teapotObject.initWithTeapotMesh(GL_STATIC_DRAW, 1.5f, nullptr);

    //
    // Our "fake" spotlights via texture projection:
    //
    spotlights[0].reset(new ProjectedSpotlight{ *this, shaderProg,
                                                Point3{ 0.0f, 3.5f,  4.0f },
                                                Point3{ 0.0f, 0.0f, -5.0f },
                                                "assets/cookie0.png", 0 });

    spotlights[1].reset(new ProjectedSpotlight{ *this, shaderProg,
                                                Point3{ 0.0f, 3.5f, 1.0f },
                                                Point3{ 0.0f, 0.0f, 5.0f },
                                                "assets/cookie1.png", 1 });
}

void ProjTexApp::drawTeapot(const double elapsedTimeSeconds)
{
    teaporRotationDegreesY += elapsedTimeSeconds * 10.0f; // 10 degrees per second

    Mat4 modelToWorldMatrix = Mat4::translation(Vec3{ 0.0f, -4.0f, -7.0f });
    modelToWorldMatrix *= Mat4::rotationY(degToRad(teaporRotationDegreesY));

    const Mat4 mvpMatrix = projMatrix * viewMatrix * modelToWorldMatrix; // In OGL layout p*v*m

    shaderProg.bind();
    shaderProg.setUniform1i(colorTextureLocation, 0);
    shaderProg.setUniformMat4(mvpMatrixLocation, mvpMatrix);

    const Mat4 invModelToWorldMatrix = inverse(modelToWorldMatrix);
    for (int s = 0; s < arrayLength(spotlights); ++s)
    {
        spotlights[s]->onFrameRender(invModelToWorldMatrix);
    }

    teapotTexture.bind();
    teapotObject.bindVA();
    teapotObject.draw(GL_TRIANGLES);
}

void ProjTexApp::drawGroundPlane()
{
    const Mat4 modelToWorldMatrix = Mat4::translation(Vec3{ 0.0f, -5.0f, -24.0f });
    const Mat4 mvpMatrix = projMatrix * viewMatrix * modelToWorldMatrix; // In OGL layout p*v*m

    shaderProg.bind();
    shaderProg.setUniform1i(colorTextureLocation, 0);
    shaderProg.setUniformMat4(mvpMatrixLocation, mvpMatrix);

    const Mat4 invModelToWorldMatrix = inverse(modelToWorldMatrix);
    for (int s = 0; s < arrayLength(spotlights); ++s)
    {
        spotlights[s]->onFrameRender(invModelToWorldMatrix);
    }

    groundTexture.bind();
    groundObject.bindVA();
    groundObject.draw(GL_TRIANGLES);
}

void ProjTexApp::onFrameRender(const std::int64_t /* currentTimeMillis */,
                               const std::int64_t elapsedTimeMillis)
{
    for (int s = 0; s < arrayLength(spotlights); ++s)
    {
        spotlights[s]->animate(millisToSeconds(elapsedTimeMillis));
    }

    drawGroundPlane();
    drawTeapot(millisToSeconds(elapsedTimeMillis));
}

// ========================================================
// AppFactory:
// ========================================================

GLFWApp::Ptr AppFactory::createGLFWAppInstance()
{
    return GLFWApp::Ptr{ new ProjTexApp() };
}

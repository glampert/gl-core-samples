
// ================================================================================================
// -*- C++ -*-
// File: doom3md5.hpp
// Author: Guilherme R. Lampert
// Created on: 06/02/16
// Brief: Loader from DOOM 3 MD5 models and animations.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#ifndef DOOM3MD5_HPP
#define DOOM3MD5_HPP

#include "gl_utils.hpp"

#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <fstream>

namespace DOOM3
{

class MaterialInstance;
class ModelInstance;
class AnimInstance;

// ========================================================
// DOOM 3 MD5 model and animation data structures:
// ========================================================

struct BoundingBox
{
    Point3 mins;
    Point3 maxs;
};

struct Triangle
{
    int index[3];
};

struct Vertex
{
    float u;
    float v;
    int firstWeight;
    int weightCount;
};

struct Joint
{
    Quat orient;
    Point3 pos;
    int parent;
    std::string name;
};

struct Weight
{
    Point3 pos;
    float bias;
    int joint;
};

struct Mesh
{
    // Ref to a material held by the parent model.
    const MaterialInstance * material;

    // Geometry and vertex-weighting info:
    std::vector<Triangle> triangles;
    std::vector<Vertex>   vertexes;
    std::vector<Weight>   weights;
};

// ========================================================
// class MaterialInstance:
// ========================================================

class MaterialInstance final
{
public:

    // Texture mapping units used for each texture map:
    enum TMU
    {
        TMU_Base     = 0,
        TMU_Normal   = 1,
        TMU_Specular = 2,
        TMU_Last     = TMU_Specular
    };

    // Initializes the material and loads the textures.
    MaterialInstance(GLFWApp & owner, std::string matName);

    // Copy/assignment is disabled.
    MaterialInstance(const MaterialInstance &) = delete;
    MaterialInstance & operator = (const MaterialInstance &) = delete;

    // Sets the material textures as current for subsequent rendering.
    void apply() const noexcept;

    // Read-only accessors:
    const std::string & getName()            const noexcept { return name;            }
    const GLTexture   & getBaseTexture()     const noexcept { return baseTexture;     }
    const GLTexture   & getNormalTexture()   const noexcept { return normalTexture;   }
    const GLTexture   & getSpecularTexture() const noexcept { return specularTexture; }

    // Shading params:
    float getShininess()            const noexcept { return shininess;     }
    const Vec4 & getAmbientColor()  const noexcept { return ambientColor;  }
    const Vec4 & getDiffuseColor()  const noexcept { return diffuseColor;  }
    const Vec4 & getSpecularColor() const noexcept { return specularColor; }
    const Vec4 & getEmissiveColor() const noexcept { return emissiveColor; }

private:

    // Material name is usually a file path to a texture without the extension;
    // E.g.: "models/monsters/hellknight/hellknight"
    const std::string name;

    // Set of texture maps used by DOOM 3 models:
    GLTexture baseTexture;     // base name
    GLTexture normalTexture;   // base name + _local
    GLTexture specularTexture; // base name + _s

    // Additional shading parameters:
    float shininess;
    Vec4 ambientColor;
    Vec4 diffuseColor;
    Vec4 specularColor;
    Vec4 emissiveColor;
};

// Materials are uniquely indexed by name. The map owns each instance.
using MaterialMap = std::unordered_map<std::string, std::unique_ptr<const MaterialInstance>>;

// ========================================================
// class ModelInstance:
// ========================================================

// Model/mesh/joint data loaded from a .md5mesh file.
class ModelInstance final
{
public:

    // Load model from file:
    ModelInstance(GLFWApp & owner, const std::string & filename);
    ModelInstance(GLFWApp & owner, std::istream & inStr);

    // Copy/assignment is disabled.
    ModelInstance(const ModelInstance &) = delete;
    ModelInstance & operator = (const ModelInstance &) = delete;

    // Find joint by name. Null if the name is not found.
    // Pointer belongs to the model, so never attempt to free it.
    const Joint * findJoint(const std::string & jointName) const;

    // Returns null if material not is present in this model.
    // The returned pointer belongs to the ModelInstance and should never be freed!
    const MaterialInstance * findMaterial(const std::string & matName) const;

    // Creates and register a new material. Tries to find the appropriate textures,
    // but sets defaults if they are not found. This method always returns a valid material.
    const MaterialInstance * createMaterial(const std::string & matName);

    // Read-only accessors:
    const std::vector<Mesh>  & getMeshes()    const noexcept { return meshes;    }
    const std::vector<Joint> & getJoints()    const noexcept { return joints;    }
    const MaterialMap        & getMaterials() const noexcept { return materials; }

private:

    void parseModel(std::istream & inStr);
    void parseMesh(std::istream & inStr, std::size_t meshIndex);
    void parseJoints(std::istream & inStr, std::size_t numJoints);

    // Needed to create the material textures.
    GLFWApp & app;

    std::vector<Mesh>  meshes;    // Sub-meshes with vertex positions, indexes, tex coords.
    std::vector<Joint> joints;    // Joints for skinning. AKA the skeleton. Initially the bind/home pose.
    MaterialMap        materials; // All materials (textures) referenced by this model.
};

// ========================================================
// class AnimInstance:
// ========================================================

// Animation data loaded from a .md5anim file.
class AnimInstance final
{
public:

    // Load animation data from file:
    explicit AnimInstance(const std::string & filename);
    explicit AnimInstance(std::istream & inStr);

    // Copy/assignment is disabled.
    AnimInstance(const AnimInstance &) = delete;
    AnimInstance & operator = (const AnimInstance &) = delete;

    // Read-only accessors:
    int getNumFrames() const noexcept { return numFrames; }
    int getNumJoints() const noexcept { return numJoints; }
    int getFrameRate() const noexcept { return frameRate; }

    double getDurationSeconds() const noexcept { return duration; }
    double getPlaybackSeconds() const noexcept { return static_cast<double>(numFrames) / static_cast<double>(frameRate); }

    const BoundingBox & getBoundsForFrame(const int frameIndex) const noexcept
    {
        assert(frameIndex >= 0);
        assert(frameIndex < numFrames);
        return bboxes[frameIndex];
    }
    const Joint * getJointsForFrame(const int frameIndex) const noexcept
    {
        assert(frameIndex >= 0);
        assert(frameIndex < numFrames);
        return skelFrames[frameIndex].get();
    }

private:

    // Bit-flags for HierarchyInfo::flags field.
    enum Flags
    {
        AnimTX = 1 << 0,
        AnimTY = 1 << 1,
        AnimTZ = 1 << 2,
        AnimQX = 1 << 3,
        AnimQY = 1 << 4,
        AnimQZ = 1 << 5
    };

    // An entry in the 'hierarchy' section of a md5anim.
    struct HierarchyInfo
    {
        int flags;
        int parent;
        int startIndex;
        char name[64]; // Must also accommodate a '\0' at the end.
    };

    // An entry in the 'baseframe' section of a md5anim.
    struct BaseFrameJointPose
    {
        Quat orient;
        Point3 pos;
    };

    // File processing helpers:
    void parseAnim(std::istream & inStr);
    void parseBounds(std::istream & inStr, int numBounds);
    static void parseHierarchy(std::istream & inStr, std::vector<HierarchyInfo> & hierarchy, int numJoints);
    static void parseBaseFrame(std::istream & inStr, std::vector<BaseFrameJointPose> & baseFrame, int numJoints);
    static void parseFrame(std::istream & inStr, std::vector<float> & frameData, int numAnimatedComponents);

    // Builds a skeleton for a given frame of animation data.
    // We can then use that skeleton (set of joints) to animate a ModelInstance.
    static void buildFrameSkeleton(const std::vector<HierarchyInfo> & hierarchy,
                                   const std::vector<BaseFrameJointPose> & baseFrame,
                                   const std::vector<float> & frameData,
                                   Joint * skelJointsOut, int numJoints);

    // Animation data from the md5anim file:
    int numFrames;   // Rows in the skelFrames[][] 2D array.
    int numJoints;   // Should match the model's count.
    int frameRate;   // Usually 24 fps, but doesn't have to be.
    double duration; // 1.0/frameRate.

    // Each entry in the vector is a pointer to an array of Joints.
    // We have skelFrames[numFrames][numJoints].
    std::vector<std::unique_ptr<Joint[]>> skelFrames;

    // The animation file stores the bounds of each frame. Even though we
    // aren't using them at the moment, they are loaded and saved here.
    std::vector<BoundingBox> bboxes;
};

// Animations are uniquely indexed by filename. The map owns each instance.
using AnimMap = std::unordered_map<std::string, std::unique_ptr<const AnimInstance>>;

// ========================================================
// Light helper classes:
// ========================================================

// NOTE: We have the same constant in the shaders, so they must match!!!
constexpr int MaxLights = 2;

// Interface for the available light types.
class LightBase
{
public:

    enum Type
    {
        PointLight = 0,
        Flashlight = 1
    };

    virtual Point3 getWorldSpacePosition() const = 0;
    virtual Point3 getModelSpacePosition() const = 0;
    virtual Type getType() const = 0;
    virtual ~LightBase() = default;
};

// Simple point light source.
class PointLightSource final
    : public LightBase
{
public:

    Vec4 color { 0.5f, 0.5f, 0.5f, 1.0f };
    Point3 positionWorldSpace { 0.0f, 0.0f, 0.0f };
    Point3 positionModelSpace { 0.0f, 0.0f, 0.0f };

    // Point light attenuation parameters:
    float radius         { 10.0f };
    float attenConst     { 1.0f  };
    float attenLinear    { 2.0f / radius };
    float attenQuadratic { 1.0f / (radius * radius) };

    Point3 getWorldSpacePosition() const override { return positionWorldSpace; }
    Point3 getModelSpacePosition() const override { return positionModelSpace; }
    Type getType() const override { return PointLight; }
};

// DOOM3-like flashlight using a projected texture (see the projected spotlight demo).
class FlashlightSource final
    : public LightBase
{
public:

    // color.w is the flashlight falloff scale.
    Vec4 color { 0.5f, 0.5f, 0.5f, 1.0f / 50.0f };

    // Cached positions:
    Point3 positionWorldSpace { 0.0f, 0.0f, 0.0f };
    Point3 positionModelSpace { 0.0f, 0.0f, 0.0f };
    Point3 lookAtWorldSpace   { 0.0f, 0.0f, 0.0f };

    // Matrix to view the scene from the light's perspective:
    Mat4 lightPerspectiveMatrix { Mat4::identity() };
    Mat4 lightProjectionMatrix  { Mat4::identity() };

    // References an external texture with the projected pattern.
    const GLTexture * lightCookieTexture { nullptr };

    // Computes the light projection matrix needed in the shader.
    void computeProjectionMatrix(const Mat4 & invModelToWorldMatrix)
    {
        const auto lookAtModelSpace = worldPointToModel(invModelToWorldMatrix, lookAtWorldSpace);
        const auto viewMatrix = Mat4::lookAt(positionModelSpace, lookAtModelSpace, Vec3::yAxis());

        // Bias matrix is a constant.
        // It performs a linear transformation to go from the [–1, 1]
        // range to the [0, 1] range. Having the coordinates in the [0, 1]
        // range is necessary for the values to be used as texture coordinates.
        const auto biasMatrix = Mat4{ Vec4{ 0.5f,  0.0f,  0.0f,  0.0f },
                                      Vec4{ 0.0f, -0.5f,  0.0f,  0.0f },
                                      Vec4{ 0.5f,  0.5f,  1.0f,  0.0f },
                                      Vec4{ 0.0f,  0.0f,  0.0f,  1.0f } };

        // The combined light view, perspective and bias matrices to generate the projected texture coordinates.
        lightProjectionMatrix = biasMatrix * lightPerspectiveMatrix * viewMatrix;
    }

    Point3 getWorldSpacePosition() const override { return positionWorldSpace; }
    Point3 getModelSpacePosition() const override { return positionModelSpace; }
    Type getType() const override { return Flashlight; }
};

// ========================================================
// class AnimatedEntity:
// ========================================================

// Encompasses a DOOM 3 MD5 model, its animations and associated render data.
class AnimatedEntity final
{
public:

    // Load the model from a .md5mesh file and the specified set of .md5anim files:
    AnimatedEntity(GLFWApp & owner, const std::string & modelFile,
                   const std::vector<std::string> & animFiles);

    // Copy/assignment is disabled.
    AnimatedEntity(const AnimatedEntity &) = delete;
    AnimatedEntity & operator = (const AnimatedEntity &) = delete;

    // Find by filename. Returns null if the animation is not present in this entity.
    // The returned pointer belongs to the entity and should never be freed!
    const AnimInstance * findAnimation(const std::string & animName) const;

    // Set the active animation, starting at the beginning and overriding current states.
    void setAnimation(const std::string & animName);
    void setAnimation(const AnimInstance * anim);

    // Test if the given animation can be applied to the model this entity has.
    bool checkAnimationValidity(const AnimInstance & anim) const;

    // Perform animation state update, calculating the current and next frames, given a delta time.
    // Returns the current loop count. Every time a full run of the animation is completed, the counter is incremented.
    int updateAnimation(double elapsedTimeSeconds);

    // Updates each mesh with the current joint skeleton and sends the new data to the GL.
    // This performs "CPU skinning" in the model. Should be called right after updateAnimation().
    void updateModelPose();

    // Draw the whole model using a provided material. Will use the current pose,
    // which is the bind pose if no CPU-side animation was applied.
    void drawWholeModel(GLenum renderMode, const Mat4 & mvpMatrix, const Point3 eyePosModelSpace,
                        const MaterialInstance * material, const LightBase ** lights, int numLights);

    // Draws a simple plane-projected shadow of the whole model using a cheap shader.
    void drawWholeModelShadow(const Mat4 & shadowMvp, const Point3 & lightPosModelSpace);

    // Visual debugging helper: Adds lines for the skeleton joints, with a point
    // at the position of each joint, if the point renderer is not null.
    void addSkeletonWireFrame(GLBatchLineRenderer  * lineRenderer,
                              GLBatchPointRenderer * pointRenderer) const;

    // Visual debugging helper: Adds a line trio for each normal, tangent and bi-tangent.
    void addTangentBasis(GLBatchLineRenderer  * lineRenderer,
                         GLBatchPointRenderer * pointRenderer) const;

    // Read-only accessors:
    int getCurrentAnimFrame() const noexcept { return currFrame; }
    int getAnimLoopCount()    const noexcept { return loopCount; }
    const ModelInstance & getModelInstance() const noexcept { return model; }

private:

    // Internal helpers:
    void loadShaderProgram(GLFWApp & app);
    void loadAnimations(GLFWApp & app, const std::vector<std::string> & animFiles);
    void setUpInitialVertexArray();
    void applyLight(const LightBase & light, int index);

    // Applies a set of skeleton joints/frames to the mesh vertexes, generating OpenGL
    // render data from it. This is our "CPU skinning" variant for quick testing.
    static void animateMesh(const Mesh & mesh,
                            const std::vector<Joint>  & skelJoints,
                            std::vector<GLDrawVertex> * vertsOut,
                            std::vector<GLDrawIndex>  * indexesOut);

    // Smoothly interpolate two skeletons/joint-sets. We can then apply the
    // resulting joint-set to a model using animateMesh() or GPU skinning.
    static void interpolateSkeletons(const Joint * skelA, const Joint * skelB,
                                     int numJoints, float interp,
                                     std::vector<Joint> & skelOut);

    // Uniform var locations from GL:
    struct ShaderUniforms
    {
        // Per-object parameters:
        GLint mvpMatrixLoc;
        GLint eyePosModelSpaceLoc;

        // Texture samplers:
        GLint baseTextureLoc;
        GLint normalTextureLoc;
        GLint specularTextureLoc;

        // Material parameters:
        GLint shininessLoc;
        GLint ambientColorLoc;
        GLint diffuseColorLoc;
        GLint specularColorLoc;
        GLint emissiveColorLoc;

        // Light parameters for each available light:
        GLint numOfLightsLoc;
        GLint lightTypeLoc[MaxLights];
        GLint lightPosModelSpaceLoc[MaxLights];
        GLint lightAttenConstLoc[MaxLights];
        GLint lightAttenLinearLoc[MaxLights];
        GLint lightAttenQuadraticLoc[MaxLights];
        GLint lightColorLoc[MaxLights];
        GLint lightProjectionMatrixLoc[MaxLights];
        GLint lightCookieTextureLoc[MaxLights];

        // Projected shadow parameters:
        GLuint shadowMvpMatrixLoc;
        GLuint shadowLightPosLoc;
        GLuint shadowParamsLoc;
    };

    // DOOM 3 models use a pretty large scale, so we shrink them down a bit.
    static constexpr float ModelScale = 0.07f;

    // The immutable model data:
    ModelInstance model;

    // Set of registered animations, loaded from md5anim files.
    AnimMap animations;

    // Animation playback states:
    int currFrame;
    int loopCount;
    double lastTimeSec;
    const AnimInstance * currAnim;
    std::vector<Joint> currSkeleton;

    // GL draw vertexes and indexes after applying an animation.
    // The contents of these arrays match the OpenGL vertex/index buffers.
    std::vector<GLDrawVertex> finalVerts;
    std::vector<GLDrawIndex>  finalIndexes;

    // Aux GL render data:
    GLVertexArray  vertArray;
    GLShaderProg   shaderProg;
    GLShaderProg   shadowProg;
    ShaderUniforms shaderVars;
};

// ========================================================
// Quaternion math helpers:
// ========================================================

float quaternionComputeW(float x, float y, float z);
Quat quaternionMulXYZ(const Quat & quat, const Point3 & point);
Point3 quaternionRotatePoint(const Quat & quat, const Point3 & point);

} // namespace DOOM3 {}

#endif // DOOM3MD5_HPP

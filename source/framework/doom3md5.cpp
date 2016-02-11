
// ================================================================================================
// -*- C++ -*-
// File: doom3md5.cpp
// Author: Guilherme R. Lampert
// Created on: 06/02/16
// Brief: Loader for DOOM 3 MD5 models and animations.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#include "doom3md5.hpp"

#include <array>
#include <cstring>
#include <cstdio>

//
// Two relevant sources of information about the MD5Mesh and MD5Anim formats:
//
// This article, which was released before DOOM3 went Open Source
// and I believe was one of the first to fully reverse-engineer
// these file formats used by the game:
//
// http://tfc.duke.free.fr/coding/md5-specs-en.html
//
// And of course, now the source code for DOOM3 and DOOM3-BFG is available:
//
// https://github.com/id-Software/DOOM-3
// https://github.com/id-Software/DOOM-3-BFG
//

namespace DOOM3
{

// Swizzle id-style coordinates to GL layout:
static void swapYZ(Point3 & p)
{
    const float y = p[1];
    p[1] = p[2];
    p[2] = y;
}

// ========================================================
// class MaterialInstance:
// ========================================================

MaterialInstance::MaterialInstance(GLFWApp & owner, std::string matName)
    : name            { std::move(matName) }
    , baseTexture     { owner }
    , normalTexture   { owner }
    , specularTexture { owner }
    , shininess       { 50.0f }
    , ambientColor    { 0.2f, 0.2f, 0.2f, 1.0f }
    , diffuseColor    { 1.0f, 1.0f, 1.0f, 1.0f }
    , specularColor   { 0.5f, 0.5f, 0.5f, 1.0f }
    , emissiveColor   { 0.0f, 0.0f, 0.0f, 1.0f }
{
    std::string texName;

    texName = name + ".tga";
    baseTexture.initFromFile(texName, false, GLTexture::Filter::LinearMipmaps, GLTexture::WrapMode::Clamp, true, TMU_Base);

    texName = name + "_local.tga";
    normalTexture.initFromFile(texName, false, GLTexture::Filter::LinearMipmaps, GLTexture::WrapMode::Clamp, true, TMU_Normal);

    texName = name + "_s.tga";
    specularTexture.initFromFile(texName, false, GLTexture::Filter::LinearMipmaps, GLTexture::WrapMode::Clamp, true, TMU_Specular);
}

void MaterialInstance::apply() const noexcept
{
    baseTexture.bind();
    normalTexture.bind();
    specularTexture.bind();
}

// ========================================================
// class ModelInstance:
// ========================================================

ModelInstance::ModelInstance(GLFWApp & owner, const std::string & filename)
    : app{ owner }
{
    std::ifstream inFile{ filename };
    if (!inFile.is_open())
    {
        throw std::runtime_error{ "Unable to open file \"" + filename + "\"!" };
    }

    parseModel(inFile);
    app.printF("DOOM 3 model instance \"%s\" loaded. Meshes: %zu, joints: %zu, materials: %zu.",
               filename.c_str(), meshes.size(), joints.size(), materials.size());
}

ModelInstance::ModelInstance(GLFWApp & owner, std::istream & inStr)
    : app{ owner }
{
    parseModel(inStr);
}

void ModelInstance::parseModel(std::istream & inStr)
{
    int versionNum = 0;
    int numJoints  = 0;
    int numMeshes  = 0;
    int currMesh   = 0;
    std::array<char, 1024> lineBuffer;

    while (!inStr.eof())
    {
        inStr.getline(lineBuffer.data(), lineBuffer.size());

        if (std::sscanf(lineBuffer.data(), "MD5Version %i", &versionNum) == 1)
        {
            // Models from DOOM 3 should have version 10.
            if (versionNum != 10)
            {
                throw std::runtime_error{ "Bad model version! Expected 10, got " + std::to_string(versionNum) };
            }
        }
        else if (std::sscanf(lineBuffer.data(), "numJoints %i", &numJoints) == 1)
        {
            if (numJoints > 0)
            {
                // Preallocate memory for the base skeleton joints:
                joints.resize(numJoints);
            }
        }
        else if (std::sscanf(lineBuffer.data(), "numMeshes %i", &numMeshes) == 1)
        {
            if (numMeshes > 0)
            {
                // Preallocate memory for the meshes:
                meshes.resize(numMeshes);
            }
        }
        else if (std::strncmp(lineBuffer.data(), "joints {", 8) == 0)
        {
            parseJoints(inStr, numJoints);
        }
        else if (std::strncmp(lineBuffer.data(), "mesh {", 6) == 0)
        {
            parseMesh(inStr, currMesh++);
        }
    }
}

void ModelInstance::parseMesh(std::istream & inStr, const std::size_t meshIndex)
{
    if (meshIndex >= meshes.size())
    {
        throw std::runtime_error{ "Bad mesh index! " + std::to_string(meshIndex) };
    }

    auto & mesh = meshes[meshIndex];

    int vertIndex   = 0;
    int triIndex    = 0;
    int weightIndex = 0;
    int numVerts    = 0;
    int numTris     = 0;
    int numWeights  = 0;

    std::string materialName;
    std::array<int,   3>    iData;
    std::array<float, 4>    fData;
    std::array<char,  1024> lineBuffer;

    lineBuffer.fill('\0');

    // Seek the end of the { } block:
    while (!inStr.eof() && lineBuffer[0] != '}')
    {
        inStr.getline(lineBuffer.data(), lineBuffer.size());

        // Material/texture available?
        if (std::strstr(lineBuffer.data(), "shader "))
        {
            int quote = 0;
            materialName.clear();

            // Copy the shader name without the quote marks:
            for (std::size_t i = 0; i < lineBuffer.size() && quote < 2; ++i)
            {
                if (lineBuffer[i] == '\"')
                {
                    ++quote;
                }
                if (quote == 1 && lineBuffer[i] != '\"')
                {
                    materialName += lineBuffer[i];
                }
            }

            mesh.material = findMaterial(materialName);
            if (mesh.material == nullptr)
            {
                mesh.material = createMaterial(materialName);
            }
        }
        else if (std::sscanf(lineBuffer.data(), " numverts %i", &numVerts) == 1)
        {
            if (numVerts > 0)
            {
                // Preallocate memory for the mesh vertexes:
                mesh.vertexes.resize(numVerts);
            }
        }
        else if (std::sscanf(lineBuffer.data(), " numtris %i", &numTris) == 1)
        {
            if (numTris > 0)
            {
                // Preallocate memory for the triangles:
                mesh.triangles.resize(numTris);
            }
        }
        else if (std::sscanf(lineBuffer.data(), " numweights %i", &numWeights) == 1)
        {
            if (numWeights > 0)
            {
                // Preallocate memory for vertex the weights:
                mesh.weights.resize(numWeights);
            }
        }
        else if (std::sscanf(lineBuffer.data(), " vert %i ( %f %f ) %d %d", &vertIndex,
                             &fData[0], &fData[1], &iData[0], &iData[1]) == 5)
        {
            mesh.vertexes[vertIndex].u = fData[0];
            mesh.vertexes[vertIndex].v = fData[1];
            mesh.vertexes[vertIndex].firstWeight = iData[0];
            mesh.vertexes[vertIndex].weightCount = iData[1];
        }
        else if (std::sscanf(lineBuffer.data(), " tri %i %i %i %i", &triIndex,
                             &iData[0], &iData[1], &iData[2]) == 4)
        {
            mesh.triangles[triIndex].index[0] = iData[0];
            mesh.triangles[triIndex].index[1] = iData[1];
            mesh.triangles[triIndex].index[2] = iData[2];
        }
        else if (std::sscanf(lineBuffer.data(), " weight %i %i %f ( %f %f %f )", &weightIndex,
                             &iData[0], &fData[3], &fData[0], &fData[1], &fData[2]) == 6)
        {
            mesh.weights[weightIndex].pos[0] = fData[0];
            mesh.weights[weightIndex].pos[1] = fData[1];
            mesh.weights[weightIndex].pos[2] = fData[2];
            mesh.weights[weightIndex].bias   = fData[3];
            mesh.weights[weightIndex].joint  = iData[0];
        }
    }
}

void ModelInstance::parseJoints(std::istream & inStr, const std::size_t numJoints)
{
    int parentIndex;
    std::array<float, 3>   pos;
    std::array<float, 4>   quat;
    std::array<char, 512>  name;
    std::array<char, 1024> lineBuffer;

    for (std::size_t j = 0; j < numJoints; ++j)
    {
        if (!inStr.getline(lineBuffer.data(), lineBuffer.size()))
        {
            throw std::runtime_error{ "Unexpected EOF while parsing model joints!" };
        }

        if (std::sscanf(lineBuffer.data(), "%s %i ( %f %f %f ) ( %f %f %f )",
                        name.data(), &parentIndex,
                        &pos[0],  &pos[1],  &pos[2],
                        &quat[0], &quat[1], &quat[2]) != 8)
        {
            throw std::runtime_error{ "Error parsing joint #" + std::to_string(j) };
        }

        // W component of the quaternion is not stored.
        quat[3] = quaternionComputeW(quat[0], quat[1], quat[2]);

        // Strip quotes if needed:
        char * nameStr = name.data();
        if (nameStr[0] == '"')
        {
            nameStr[std::strlen(nameStr) - 1] = '\0';
            nameStr++;
        }

        // Store:
        auto & joint = joints[j];
        joint.orient = Quat{ quat[0], quat[1], quat[2], quat[3] };
        joint.pos    = Point3{ pos[0], pos[1], pos[2] };
        joint.parent = parentIndex;
        joint.name   = nameStr;
    }
}

const Joint * ModelInstance::findJoint(const std::string & jointName) const
{
    for (const auto & joint : joints)
    {
        if (joint.name == jointName)
        {
            return &joint;
        }
    }
    return nullptr;
}

const MaterialInstance * ModelInstance::findMaterial(const std::string & matName) const
{
    auto iter = materials.find(matName);
    if (iter == std::end(materials))
    {
        return nullptr;
    }
    return iter->second.get();
}

const MaterialInstance * ModelInstance::createMaterial(const std::string & matName)
{
    auto result = materials.emplace(matName, std::make_unique<MaterialInstance>(app, matName));
    if (result.second == false)
    {
        throw std::runtime_error{ "MaterialMap name collision! " + matName };
    }
    return result.first->second.get();
}

// ========================================================
// class AnimInstance:
// ========================================================

AnimInstance::AnimInstance(const std::string & filename)
{
    std::ifstream inFile{ filename };
    if (!inFile.is_open())
    {
        throw std::runtime_error{ "Unable to open file \"" + filename + "\"!" };
    }

    parseAnim(inFile);
}

AnimInstance::AnimInstance(std::istream & inStr)
{
    parseAnim(inStr);
}

void AnimInstance::parseAnim(std::istream & inStr)
{
    // Clear out these members first:
    numFrames = 0;
    numJoints = 0;
    frameRate = 0;
    duration  = 0;

    // Temporary locals:
    int versionNum = 0;
    int frameIndex = 0;
    int numAnimatedComponents = 0;

    std::vector<float> animFrameData;
    std::vector<HierarchyInfo> hierarchy;
    std::vector<BaseFrameJointPose> baseFrame;
    std::array<char, 1024> lineBuffer;

    while (!inStr.eof())
    {
        inStr.getline(lineBuffer.data(), lineBuffer.size());

        if (std::sscanf(lineBuffer.data(), "MD5Version %i", &versionNum) == 1)
        {
            // Models from DOOM 3 should have version 10.
            if (versionNum != 10)
            {
                throw std::runtime_error{ "Bad anim version! Expected 10, got " + std::to_string(versionNum) };
            }
        }
        else if (std::sscanf(lineBuffer.data(), " numFrames %d", &numFrames) == 1)
        {
            // Preallocate memory for skeleton frames and bounding boxes:
            if (numFrames > 0)
            {
                bboxes.resize(numFrames);
                skelFrames.resize(numFrames);
            }
        }
        else if (std::sscanf(lineBuffer.data(), " numJoints %d", &numJoints) == 1)
        {
            if (numJoints > 0)
            {
                // NOTE: This could be optimized into a single flat allocation!
                for (int f = 0; f < numFrames; ++f)
                {
                    skelFrames[f] = std::make_unique<Joint[]>(numJoints);
                }

                // Allocate temporary memory for building the skeleton frames:
                hierarchy.resize(numJoints);
                baseFrame.resize(numJoints);
            }
        }
        else if (std::sscanf(lineBuffer.data(), " numAnimatedComponents %d", &numAnimatedComponents) == 1)
        {
            if (numAnimatedComponents > 0)
            {
                // Preallocate memory for animation frame data:
                animFrameData.resize(numAnimatedComponents);
            }
        }
        else if (std::sscanf(lineBuffer.data(), " frameRate %i", &frameRate) == 1)
        {
            duration = 1.0 / static_cast<double>(frameRate);
        }
        else if (std::strncmp(lineBuffer.data(), "hierarchy {", 11) == 0)
        {
            parseHierarchy(inStr, hierarchy, numJoints);
        }
        else if (std::strncmp(lineBuffer.data(), "bounds {", 8) == 0)
        {
            parseBounds(inStr, numFrames);
        }
        else if (std::strncmp(lineBuffer.data(), "baseframe {", 11) == 0)
        {
            parseBaseFrame(inStr, baseFrame, numJoints);
        }
        else if (std::sscanf(lineBuffer.data(), "frame %i {", &frameIndex) == 1)
        {
            parseFrame(inStr, animFrameData, numAnimatedComponents);
            buildFrameSkeleton(hierarchy, baseFrame, animFrameData,
                               skelFrames[frameIndex].get(), numJoints);
        }
    }
}

void AnimInstance::parseBounds(std::istream & inStr, const int numBounds)
{
    std::array<float, 3> bbMin;
    std::array<float, 3> bbMax;
    std::array<char, 1024> lineBuffer;

    // One bounding-box for each frame of animation.
    for (int b = 0; b < numBounds; ++b)
    {
        if (!inStr.getline(lineBuffer.data(), lineBuffer.size()))
        {
            throw std::runtime_error{ "Unexpected EOF while parsing animation frame bounds!" };
        }

        // ( min ) ( max )
        if (std::sscanf(lineBuffer.data(), " ( %f %f %f ) ( %f %f %f )",
                        &bbMin[0], &bbMin[1], &bbMin[2], &bbMax[0], &bbMax[1], &bbMax[2]) != 6)
        {
            throw std::runtime_error{ "Error parsing bounds entry #" + std::to_string(b) };
        }

        bboxes[b].mins = Point3{ bbMin[0], bbMin[1], bbMin[2] };
        bboxes[b].maxs = Point3{ bbMax[0], bbMax[1], bbMax[2] };
    }
}

void AnimInstance::parseHierarchy(std::istream & inStr, std::vector<HierarchyInfo> & hierarchy, const int numJoints)
{
    std::array<char, 1024> lineBuffer;

    // There should be one entry for each model joint.
    for (int j = 0; j < numJoints; ++j)
    {
        if (!inStr.getline(lineBuffer.data(), lineBuffer.size()))
        {
            throw std::runtime_error{ "Unexpected EOF while parsing animation hierarchy!" };
        }

        if (std::sscanf(lineBuffer.data(), " %s %i %i %i",
                        hierarchy[j].name,   &hierarchy[j].parent,
                        &hierarchy[j].flags, &hierarchy[j].startIndex) != 4)
        {
            throw std::runtime_error{ "Error parsing hierarchy entry #" + std::to_string(j) };
        }
    }
}

void AnimInstance::parseBaseFrame(std::istream & inStr, std::vector<BaseFrameJointPose> & baseFrame, const int numJoints)
{
    std::array<float, 3> pos;
    std::array<float, 4> quat;
    std::array<char, 1024> lineBuffer;

    // There should be one entry for each model joint.
    for (int j = 0; j < numJoints; ++j)
    {
        if (!inStr.getline(lineBuffer.data(), lineBuffer.size()))
        {
            throw std::runtime_error{ "Unexpected EOF while parsing animation baseframe!" };
        }

        // Read base frame joint ( position ) ( quaternion ):
        if (std::sscanf(lineBuffer.data(), " ( %f %f %f ) ( %f %f %f )",
                        &pos[0], &pos[1], &pos[2], &quat[0], &quat[1], &quat[2]) != 6)
        {
            throw std::runtime_error{ "Error parsing baseframe entry #" + std::to_string(j) };
        }

        // Compute the W component of the quaternion (not stored in the file):
        quat[3] = quaternionComputeW(quat[0], quat[1], quat[2]);

        // Save 'em:
        baseFrame[j].orient = Quat{ quat[0], quat[1], quat[2], quat[3] };
        baseFrame[j].pos = Point3{ pos[0], pos[1], pos[2] };
    }
}

void AnimInstance::parseFrame(std::istream & inStr, std::vector<float> & frameData, const int numAnimatedComponents)
{
    std::array<char, 1024> lineBuffer;

    for (int entry = 0; entry < numAnimatedComponents;)
    {
        if (!inStr.getline(lineBuffer.data(), lineBuffer.size()))
        {
            throw std::runtime_error{ "Unexpected EOF while parsing animation frame data!" };
        }

        // Split the string by white spaces, tabs and line terminators.
        const char * token = std::strtok(lineBuffer.data(), " \t\r\n");
        while (token != nullptr)
        {
            frameData[entry++] = std::strtof(token, nullptr);
            token = std::strtok(nullptr, " \t\r\n");
        }
    }
}

void AnimInstance::buildFrameSkeleton(const std::vector<HierarchyInfo> & hierarchy,
                                      const std::vector<BaseFrameJointPose> & baseFrame,
                                      const std::vector<float> & frameData,
                                      Joint * skelJointsOut, const int numJoints)
{
    for (int i = 0; i < numJoints; ++i)
    {
        const auto & baseJoint = baseFrame[i];
        Point3 animatedPos  = baseJoint.pos;
        Quat animatedOrient = baseJoint.orient;
        int j = 0;

        // T x y z
        if (hierarchy[i].flags & AnimTX)
        {
            animatedPos[0] = frameData[hierarchy[i].startIndex + j];
            ++j;
        }
        if (hierarchy[i].flags & AnimTY)
        {
            animatedPos[1] = frameData[hierarchy[i].startIndex + j];
            ++j;
        }
        if (hierarchy[i].flags & AnimTZ)
        {
            animatedPos[2] = frameData[hierarchy[i].startIndex + j];
            ++j;
        }

        // Q x y z
        if (hierarchy[i].flags & AnimQX)
        {
            animatedOrient[0] = frameData[hierarchy[i].startIndex + j];
            ++j;
        }
        if (hierarchy[i].flags & AnimQY)
        {
            animatedOrient[1] = frameData[hierarchy[i].startIndex + j];
            ++j;
        }
        if (hierarchy[i].flags & AnimQZ)
        {
            animatedOrient[2] = frameData[hierarchy[i].startIndex + j];
            ++j;
        }

        animatedOrient[3] = quaternionComputeW(
                                animatedOrient[0],
                                animatedOrient[1],
                                animatedOrient[2]);

        // NOTE: We assume that this joint's parent has
        // already been calculated, i.e. joint's ID should
        // never be smaller than its parent ID.
        auto & thisJoint = skelJointsOut[i];
        const int parent = hierarchy[i].parent;
        thisJoint.parent = parent;

        // Cleanup the quotes. They aren't removed by parseHierarchy().
        if (hierarchy[i].name[0] == '"')
        {
            thisJoint.name.assign(hierarchy[i].name, 1, std::strlen(hierarchy[i].name) - 2);
        }
        else
        {
            thisJoint.name = hierarchy[i].name;
        }

        if (thisJoint.parent < 0) // Is this the root (no parent)?
        {
            // Copy stuff unchanged.
            thisJoint.pos    = animatedPos;
            thisJoint.orient = animatedOrient;
        }
        else
        {
            // If it has a parent we must apply the parent's
            // rotation/orientation to the position first.
            auto & parentJoint = skelJointsOut[parent];
            const Point3 rotatedPos = quaternionRotatePoint(parentJoint.orient, animatedPos);

            // Add positions:
            thisJoint.pos[0] = rotatedPos[0] + parentJoint.pos[0];
            thisJoint.pos[1] = rotatedPos[1] + parentJoint.pos[1];
            thisJoint.pos[2] = rotatedPos[2] + parentJoint.pos[2];

            // Concatenate this rotation with the parent's:
            thisJoint.orient = parentJoint.orient * animatedOrient;
            thisJoint.orient = normalize(thisJoint.orient);
        }
    }
}

// ========================================================
// class AnimatedEntity:
// ========================================================

AnimatedEntity::AnimatedEntity(GLFWApp & owner, const std::string & modelFile,
                               const std::vector<std::string> & animFiles)
    : model       { owner, modelFile }
    , currFrame   { 0 }
    , loopCount   { 0 }
    , lastTimeSec { 0 }
    , currAnim    { nullptr }
    , vertArray   { owner   }
    , shaderProg  { owner   }
    , shadowProg  { owner   }
{
    loadShaderProgram(owner);
    loadAnimations(owner, animFiles);
    setUpInitialVertexArray();
}

void AnimatedEntity::loadShaderProgram(GLFWApp & app)
{
#define GET_UNIFORM_LOC(locName, unifName)                                                        \
    do                                                                                            \
    {                                                                                             \
        std::string strName{ (unifName) };                                                        \
        shaderVars.locName = shaderProg.getUniformLocation(strName);                              \
        if (shaderVars.locName < 0)                                                               \
        {                                                                                         \
            app.printF("WARNING! Failed to get uniform var location for '%s'!", strName.c_str()); \
        }                                                                                         \
    } while (0)

    // Load vert+frag shaders:
    shaderProg.initFromFiles("source/shaders/normalmap.vert",  "source/shaders/normalmap.frag");
    shadowProg.initFromFiles("source/shaders/projshadow.vert", "source/shaders/projshadow.frag");

    // Shadow shader parameters:
    shaderVars.shadowMvpMatrixLoc = shadowProg.getUniformLocation("u_MvpMatrix");
    shaderVars.shadowLightPosLoc  = shadowProg.getUniformLocation("u_LightPosModelSpace");
    shaderVars.shadowParamsLoc    = shadowProg.getUniformLocation("u_ShadowParams");

    // Store the uniform var locations:
    GET_UNIFORM_LOC(mvpMatrixLoc       , "u_MvpMatrix");
    GET_UNIFORM_LOC(eyePosModelSpaceLoc, "u_EyePosModelSpace");
    GET_UNIFORM_LOC(baseTextureLoc     , "u_BaseTexture");
    GET_UNIFORM_LOC(normalTextureLoc   , "u_NormalTexture");
    GET_UNIFORM_LOC(specularTextureLoc , "u_SpecularTexture");
    GET_UNIFORM_LOC(shininessLoc       , "u_MatShininess");
    GET_UNIFORM_LOC(ambientColorLoc    , "u_MatAmbientColor");
    GET_UNIFORM_LOC(diffuseColorLoc    , "u_MatDiffuseColor");
    GET_UNIFORM_LOC(specularColorLoc   , "u_MatSpecularColor");
    GET_UNIFORM_LOC(emissiveColorLoc   , "u_MatEmissiveColor");
    GET_UNIFORM_LOC(numOfLightsLoc     , "u_NumOfLights");

    // If not all lights are used by the shader, we should get invalid handles.
    for (int l = 0; l < MaxLights; ++l)
    {
        GET_UNIFORM_LOC(lightTypeLoc[l]             , "u_LightType["             + std::to_string(l) + "]");
        GET_UNIFORM_LOC(lightPosModelSpaceLoc[l]    , "u_LightPosModelSpace["    + std::to_string(l) + "]");
        GET_UNIFORM_LOC(lightAttenConstLoc[l]       , "u_LightAttenConst["       + std::to_string(l) + "]");
        GET_UNIFORM_LOC(lightAttenLinearLoc[l]      , "u_LightAttenLinear["      + std::to_string(l) + "]");
        GET_UNIFORM_LOC(lightAttenQuadraticLoc[l]   , "u_LightAttenQuadratic["   + std::to_string(l) + "]");
        GET_UNIFORM_LOC(lightColorLoc[l]            , "u_LightColor["            + std::to_string(l) + "]");
        GET_UNIFORM_LOC(lightProjectionMatrixLoc[l] , "u_LightProjectionMatrix[" + std::to_string(l) + "]");
        GET_UNIFORM_LOC(lightCookieTextureLoc[l]    , "u_LightCookieTexture["    + std::to_string(l) + "]");
    }

    // Set the texture units, these won't change:
    shaderProg.bind();
    shaderProg.setUniform1i(shaderVars.numOfLightsLoc,     0); // Set to zero for safety.
    shaderProg.setUniform1i(shaderVars.baseTextureLoc,     MaterialInstance::TMU_Base);
    shaderProg.setUniform1i(shaderVars.normalTextureLoc,   MaterialInstance::TMU_Normal);
    shaderProg.setUniform1i(shaderVars.specularTextureLoc, MaterialInstance::TMU_Specular);

    // Light cookie textures follow the model texture on TMU #3
    for (int l = 0; l < MaxLights; ++l)
    {
        shaderProg.setUniform1i(shaderVars.lightCookieTextureLoc[l], MaterialInstance::TMU_Last + 1);
    }

    CHECK_GL_ERRORS(&app);

#undef GET_UNIFORM_LOC
}

void AnimatedEntity::loadAnimations(GLFWApp & app, const std::vector<std::string> & animFiles)
{
    for (const auto & animName : animFiles)
    {
        auto result = animations.emplace(animName, std::make_unique<AnimInstance>(animName));
        const auto anim = result.first->second.get();

        if (result.second == false)
        {
            throw std::runtime_error{ "AnimMap name collision! " + animName };
        }

        app.printF("DOOM 3 animation instance \"%s\" loaded. "
                   "Frames: %i, joints: %i, fps: %i, playback: %fs, duration: %fs.",
                   animName.c_str(), anim->getNumFrames(), anim->getNumJoints(),
                   anim->getFrameRate(), anim->getPlaybackSeconds(), anim->getDurationSeconds());

        if (!checkAnimationValidity(*anim))
        {
            app.printF("WARNING! Animation \"%s\" is not compatible with the entity's model!", animName.c_str());
            // Allow it to proceed. Will likely crash when attempting to use the animation...
        }
    }
}

void AnimatedEntity::setUpInitialVertexArray()
{
    const auto & meshes = model.getMeshes();
    const auto & joints = model.getJoints();

    for (const auto & mesh : meshes)
    {
        animateMesh(mesh, joints, &finalVerts, &finalIndexes);
    }

    assert(!finalVerts.empty());
    assert(!finalIndexes.empty());

    // Generate the dynamic per-vertex data:
    deriveNormalsAndTangents(finalVerts.data(),   finalVerts.size(),
                             finalIndexes.data(), finalIndexes.size(),
                             finalVerts.data());

    // We'll use this to store intermediate joint frames of animation.
    // The model's skeleton/joint-set remains with the bind pose.
    currSkeleton = joints;

    // Set up the static GL vertex array:
    vertArray.initFromData(finalVerts.data(),   finalVerts.size(),
                           finalIndexes.data(), finalIndexes.size(),
                           GL_DYNAMIC_DRAW, GLVertexLayout::Triangles);
}

bool AnimatedEntity::checkAnimationValidity(const AnimInstance & anim) const
{
    const auto & modelJoints = model.getJoints();

    // md5mesh and md5anim must have the same number of joints.
    if (modelJoints.size() != static_cast<unsigned>(anim.getNumJoints()))
    {
        return false;
    }

    // Only frame[0] of the animation is actually tested, to keep things simple and fast.
    for (std::size_t i = 0; i < modelJoints.size(); ++i)
    {
        // Joints must have the same parent index:
        if (modelJoints[i].parent != anim.getJointsForFrame(0)[i].parent)
        {
            return false;
        }

        // Joints must have the same name:
        if (modelJoints[i].name != anim.getJointsForFrame(0)[i].name)
        {
            return false;
        }
    }

    return true; // Compatible.
}

void AnimatedEntity::animateMesh(const Mesh & mesh,
                                 const std::vector<Joint>  & skelJoints,
                                 std::vector<GLDrawVertex> * vertsOut,
                                 std::vector<GLDrawIndex>  * indexesOut)
{
    if (indexesOut != nullptr)
    {
        indexesOut->clear();
        indexesOut->reserve(mesh.triangles.size() * 3);

        // Triangle indexes are copied as is:
        for (const auto & tri : mesh.triangles)
        {
            indexesOut->push_back(tri.index[0]);
            indexesOut->push_back(tri.index[1]);
            indexesOut->push_back(tri.index[2]);
        }
    }

    if (vertsOut != nullptr)
    {
        vertsOut->clear();

        // Build the final vertex position for the bind pose:
        for (const auto & vert : mesh.vertexes)
        {
            Point3 finalVertexPos{ 0.0f, 0.0f, 0.0f };

            // Calculate final vertex from joint+weights:
            for (int w = 0; w < vert.weightCount; ++w)
            {
                const auto & weight = mesh.weights[vert.firstWeight + w];
                const auto & joint  = skelJoints[weight.joint];

                // Calculate transformed vertex for this weight:
                const auto weightedVertexPos = quaternionRotatePoint(joint.orient, weight.pos);

                // The sum of all weight biases should be 1.0!
                finalVertexPos[0] += (joint.pos[0] + weightedVertexPos[0]) * weight.bias;
                finalVertexPos[1] += (joint.pos[1] + weightedVertexPos[1]) * weight.bias;
                finalVertexPos[2] += (joint.pos[2] + weightedVertexPos[2]) * weight.bias;
            }

            // Swizzle Y-Z.
            // idSoftware historically used this different axis layout.
            GLDrawVertex drawVert;
            drawVert.px = finalVertexPos[0] * ModelScale;
            drawVert.py = finalVertexPos[2] * ModelScale;
            drawVert.pz = finalVertexPos[1] * ModelScale;

            // Texture coordinates:
            drawVert.u = vert.u;
            drawVert.v = vert.v;

            // Default color:
            drawVert.r = 1.0f;
            drawVert.g = 1.0f;
            drawVert.b = 1.0f;
            drawVert.a = 1.0f;

            vertsOut->push_back(drawVert);
        }
    }
}

void AnimatedEntity::interpolateSkeletons(const Joint * skelA, const Joint * skelB,
                                          const int numJoints, float interp,
                                          std::vector<Joint> & skelOut)
{
    assert(static_cast<unsigned>(numJoints) == skelOut.size());

    // Ensure between [0,1]:
    interp = clamp(interp, 0.0f, 1.0f);

    for (int j = 0; j < numJoints; ++j)
    {
        skelOut[j].parent = skelA[j].parent;

        // Linear interpolation for position:
        skelOut[j].pos = lerp(interp, skelA[j].pos, skelB[j].pos);

        // Spherical Linear interpolation for orientation:
        skelOut[j].orient = slerp(interp, skelA[j].orient, skelB[j].orient);
    }
}

const AnimInstance * AnimatedEntity::findAnimation(const std::string & animName) const
{
    auto iter = animations.find(animName);
    if (iter == std::end(animations))
    {
        return nullptr;
    }
    return iter->second.get();
}

void AnimatedEntity::setAnimation(const std::string & animName)
{
    const auto anim = findAnimation(animName);
    if (anim == nullptr)
    {
        throw std::runtime_error{ "Animation \"" + animName + "\" doesn't belong to this entity!" };
    }
    setAnimation(anim);
}

void AnimatedEntity::setAnimation(const AnimInstance * anim)
{
    // Passing a null animation implicitly restores the bind-pose.
    if (anim == nullptr)
    {
        currSkeleton = model.getJoints();
        updateModelPose();
    }

    currFrame   = 0;
    loopCount   = 0;
    lastTimeSec = 0;
    currAnim    = anim;
}

int AnimatedEntity::updateAnimation(const double elapsedTimeSeconds)
{
    if (currAnim == nullptr)
    {
        return 0;
    }

    const auto durationSec = currAnim->getDurationSeconds();
    const auto numFrames   = currAnim->getNumFrames();

    lastTimeSec += elapsedTimeSeconds;

    // Move to next frame:
    if (lastTimeSec >= durationSec)
    {
        lastTimeSec = lastTimeSec - durationSec;
        ++currFrame;

        if (currFrame >= numFrames)
        {
            currFrame = 0;
            ++loopCount;
        }
    }

    int nextFrame = currFrame + 1;
    if (nextFrame >= numFrames)
    {
        nextFrame = numFrames - 1;
    }

    // Interpolate skeleton joints between two frames:
    interpolateSkeletons(currAnim->getJointsForFrame(currFrame), currAnim->getJointsForFrame(nextFrame),
                         currAnim->getNumJoints(), (lastTimeSec * currAnim->getFrameRate()), currSkeleton);

    // Caller can use this to test if the animation has completed.
    return loopCount;
}

void AnimatedEntity::updateModelPose()
{
    if (currAnim == nullptr)
    {
        return;
    }

    const auto & meshes = model.getMeshes();
    for (const auto & mesh : meshes)
    {
        animateMesh(mesh, currSkeleton, &finalVerts, nullptr);
    }

    // Generate the dynamic per-vertex data:
    deriveNormalsAndTangents(finalVerts.data(),   finalVerts.size(),
                             finalIndexes.data(), finalIndexes.size(),
                             finalVerts.data());

    // We only need to update the vertex buffer this time.
    vertArray.bindVA();
    vertArray.bindVB();
    vertArray.updateRawData(finalVerts.data(), finalVerts.size(), sizeof(GLDrawVertex), nullptr, 0, 0);
    vertArray.bindNull();
}

void AnimatedEntity::drawWholeModel(const GLenum renderMode, const Mat4 & mvpMatrix, const Point3 eyePosModelSpace,
                                    const MaterialInstance * material, const LightBase ** lights, int numLights)
{
    if (numLights > MaxLights)
    {
        numLights = MaxLights;
    }

    shaderProg.bind();
    shaderProg.setUniform1i(shaderVars.numOfLightsLoc, numLights);
    shaderProg.setUniformMat4(shaderVars.mvpMatrixLoc, mvpMatrix);
    shaderProg.setUniformPoint3(shaderVars.eyePosModelSpaceLoc, eyePosModelSpace);

    if (material == nullptr)
    {
        // Use whatever is the first one available.
        material = model.getMaterials().begin()->second.get();
    }

    material->apply();
    shaderProg.setUniform1f(shaderVars.shininessLoc,       material->getShininess());
    shaderProg.setUniformVec4(shaderVars.ambientColorLoc,  material->getAmbientColor());
    shaderProg.setUniformVec4(shaderVars.diffuseColorLoc,  material->getDiffuseColor());
    shaderProg.setUniformVec4(shaderVars.specularColorLoc, material->getSpecularColor());
    shaderProg.setUniformVec4(shaderVars.emissiveColorLoc, material->getEmissiveColor());

    for (int l = 0; l < numLights; ++l)
    {
        if (lights[l] != nullptr)
        {
            applyLight(*lights[l], l);
        }
    }

    vertArray.bindVA();
    vertArray.draw(renderMode);
    vertArray.bindNull();
}

void AnimatedEntity::drawWholeModelShadow(const Mat4 & shadowMvp, const Point3 & lightPosModelSpace)
{
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shadowProg.bind();
    shadowProg.setUniformMat4(shaderVars.shadowMvpMatrixLoc, shadowMvp);
    shadowProg.setUniformVec4(shaderVars.shadowParamsLoc, Vec4{ 1.0f / 15.0f, 1.0f, 0.0f, 0.0f });
    shadowProg.setUniformPoint3(shaderVars.shadowLightPosLoc, lightPosModelSpace);

    vertArray.bindVA();
    vertArray.draw(GL_TRIANGLES);
    vertArray.bindNull();

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void AnimatedEntity::applyLight(const LightBase & light, const int index)
{
    shaderProg.setUniform1i(shaderVars.lightTypeLoc[index], light.getType());
    shaderProg.setUniformPoint3(shaderVars.lightPosModelSpaceLoc[index], light.getModelSpacePosition());

    switch (light.getType())
    {
    case LightBase::PointLight :
        {
            const auto & pointLight = static_cast<const PointLightSource &>(light);
            shaderProg.setUniform1f(shaderVars.lightAttenConstLoc[index], pointLight.attenConst);
            shaderProg.setUniform1f(shaderVars.lightAttenLinearLoc[index], pointLight.attenLinear);
            shaderProg.setUniform1f(shaderVars.lightAttenQuadraticLoc[index], pointLight.attenQuadratic);
            shaderProg.setUniformVec4(shaderVars.lightColorLoc[index], pointLight.color);
            break;
        }
    case LightBase::Flashlight :
        {
            const auto & flashlight = static_cast<const FlashlightSource &>(light);
            shaderProg.setUniformMat4(shaderVars.lightProjectionMatrixLoc[index], flashlight.lightProjectionMatrix);
            shaderProg.setUniformVec4(shaderVars.lightColorLoc[index], flashlight.color);
            if (flashlight.lightCookieTexture != nullptr)
            {
                flashlight.lightCookieTexture->bind();
            }
            break;
        }
    default :
        assert(false && "Bad light type enum!");
    } // switch (light.getType())
}

void AnimatedEntity::addSkeletonWireFrame(GLBatchLineRenderer  * lineRenderer,
                                          GLBatchPointRenderer * pointRenderer) const
{
    constexpr float pointSize = 10.0f;
    const Vec4 pointColor{ 1.0f, 1.0f, 1.0f, 1.0f }; // white
    const Vec4 lineColor { 0.0f, 1.0f, 0.0f, 1.0f }; // green

    Point3 p0, p1;
    for (const auto & joint : currSkeleton)
    {
        p0 = scale(joint.pos, ModelScale);
        swapYZ(p0); // Joint position is in id's layout. We need to swap Y-Z to draw on GL.

        if (pointRenderer != nullptr)
        {
            pointRenderer->addPoint(p0, pointSize, pointColor);
        }

        if (lineRenderer != nullptr && joint.parent >= 0)
        {
            p1 = scale(currSkeleton[joint.parent].pos, ModelScale);
            swapYZ(p1);

            lineRenderer->addLine(p0, p1, lineColor);
        }
    }
}

void AnimatedEntity::addTangentBasis(GLBatchLineRenderer  * lineRenderer,
                                     GLBatchPointRenderer * pointRenderer) const
{
    constexpr float pointSize  = 5.0f;
    constexpr float lineLength = 0.2f;

    const Vec4 cP{ 1.0f, 1.0f, 1.0f, 1.0f }; // white
    const Vec4 cN{ 0.0f, 0.0f, 1.0f, 1.0f }; // blue
    const Vec4 cT{ 1.0f, 0.0f, 0.0f, 1.0f }; // red
    const Vec4 cB{ 0.0f, 1.0f, 0.0f, 1.0f }; // green

    Point3 vN, vT, vB;
    for (const auto & vert : finalVerts)
    {
        const auto origin = Point3{ vert.px, vert.py, vert.pz };
        if (pointRenderer != nullptr)
        {
            pointRenderer->addPoint(origin, pointSize, cP);
        }

        if (lineRenderer != nullptr)
        {
            vN[0] = (vert.nx * lineLength) + vert.px;
            vN[1] = (vert.ny * lineLength) + vert.py;
            vN[2] = (vert.nz * lineLength) + vert.pz;

            vT[0] = (vert.tx * lineLength) + vert.px;
            vT[1] = (vert.ty * lineLength) + vert.py;
            vT[2] = (vert.tz * lineLength) + vert.pz;

            vB[0] = (vert.bx * lineLength) + vert.px;
            vB[1] = (vert.by * lineLength) + vert.py;
            vB[2] = (vert.bz * lineLength) + vert.pz;

            lineRenderer->addLine(origin, vN, cN);
            lineRenderer->addLine(origin, vT, cT);
            lineRenderer->addLine(origin, vB, cB);
        }
    }
}

// ========================================================
// Quaternion math helpers:
// ========================================================

float quaternionComputeW(const float x, const float y, const float z)
{
    // We know the length of a unit quaternion should
    // amount to 1, so we can figure out W from XYZ.
    const float t = 1.0f - (x * x) - (y * y) - (z * z);
    if (t < 0.0f)
    {
        return 0.0f;
    }
    return -std::sqrt(t);
}

Quat quaternionMulXYZ(const Quat & quat, const Point3 & point)
{
    return { (quat[3] * point[0]) + (quat[1] * point[2]) - (quat[2] * point[1]),
             (quat[3] * point[1]) + (quat[2] * point[0]) - (quat[0] * point[2]),
             (quat[3] * point[2]) + (quat[0] * point[1]) - (quat[1] * point[0]),
            -(quat[0] * point[0]) - (quat[1] * point[1]) - (quat[2] * point[2]) };
}

Point3 quaternionRotatePoint(const Quat & quat, const Point3 & point)
{
    // For unit quaternions we can assume the conjugate (-quat.xyz) is the same as the inverse.
    const Quat inv  = normalize(Quat{ -quat[0], -quat[1], -quat[2], quat[3] });
    const Quat temp = quaternionMulXYZ(quat, point);
    const Quat outQ = temp * inv;
    return { outQ[0], outQ[1], outQ[2] };
}

} // namespace DOOM3 {}

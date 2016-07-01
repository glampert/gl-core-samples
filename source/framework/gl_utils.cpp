
// ================================================================================================
// -*- C++ -*-
// File: gl_utils.cpp
// Author: Guilherme R. Lampert
// Created on: 14/11/15
// Brief: Simple OpenGL utilities and helpers.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#include "gl_utils.hpp"

#include <iostream>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ========================================================
// Our image file loading needs are covered by STB image.
// ========================================================

#define STB_IMAGE_IMPLEMENTATION 1
#define STB_IMAGE_STATIC         1
#define STBI_NO_HDR              1
#define STBI_NO_PIC              1
#define STBI_NO_PNM              1
#define STBI_NO_PSD              1
#define STBI_NO_LINEAR           1
#include "stb/stb_image.h"

// ========================================================
// class GLTexture:
// ========================================================

GLTexture::GLTexture(GLFWApp & owner)
    : app       { owner }
    , handle    { 0 }
    , target    { 0 }
    , tmu       { 0 }
    , width     { 0 }
    , height    { 0 }
    , filter    { Filter::Nearest }
    , wrapMode  { WrapMode::Clamp }
    , hasMipmaps{ false }
{
}

GLTexture::GLTexture(GLFWApp & owner, const std::string & imageFile,
                     const bool flipV, const Filter texFilter, const WrapMode texWrap,
                     const bool mipmaps, const int texUnit, const GLenum texTarget)
    : GLTexture{ owner }
{
    initFromFile(imageFile, flipV, texFilter, texWrap, mipmaps, texUnit, texTarget);
}

GLTexture::~GLTexture()
{
    cleanup();
}

void GLTexture::initFromFile(const std::string & imageFile, const bool flipV,
                             const Filter texFilter, const WrapMode texWrap,
                             const bool mipmaps, const int texUnit, const GLenum texTarget)
{
    assert(!imageFile.empty());

    if (isInitialized())
    {
        app.errorF("Texture already initialized! Call cleanup() first!");
    }

    stbi_set_flip_vertically_on_load(flipV);

    int imgWidth  = 0;
    int imgHeight = 0;
    int imgComps  = 0;

    // So we don't have to bother manually freeing it...
    auto deleter = [](stbi_uc * rawPtr) {
        stbi_image_free(rawPtr);
    };

    std::unique_ptr<stbi_uc[], decltype(deleter)> data{
            stbi_load(imageFile.c_str(), &imgWidth, &imgHeight, &imgComps, 4),
            deleter };

    if (data == nullptr)
    {
        app.errorF("Unable to load texture image \"%s\": %s",
                   imageFile.c_str(), stbi_failure_reason());
    }

    initFromData(data.get(), imgWidth, imgHeight, 4, texFilter,
                 texWrap, mipmaps, texUnit, texTarget);

    app.printF("New texture loaded from file \"%s\" (%dx%d).",
               imageFile.c_str(), width, height);
}

void GLTexture::initFromData(const std::uint8_t * data, const int w, const int h,
                             const int chans, const Filter texFilter, const WrapMode texWrap,
                             bool mipmaps, const int texUnit, const GLenum texTarget)
{
    assert(data != nullptr);
    assert(w > 0 && h > 0);
    assert(chans == 4 && "Only RGBA currently supported!");

    (void)chans; // Might add support in the future...

    if (isInitialized())
    {
        app.errorF("Texture already initialized! Call cleanup() first!");
    }

    GLuint glTexHandle = 0;
    glGenTextures(1, &glTexHandle);

    if (glTexHandle == 0)
    {
        app.errorF("Failed to allocate a new GL texture handle! Possibly out-of-memory!");
    }

    glActiveTexture(GL_TEXTURE0 + texUnit);
    glBindTexture(texTarget, glTexHandle);

    glTexImage2D(
        /* target   = */ texTarget,
        /* level    = */ 0,
        /* internal = */ GL_RGBA,
        /* width    = */ w,
        /* height   = */ h,
        /* border   = */ 0,
        /* format   = */ GL_RGBA,
        /* type     = */ GL_UNSIGNED_BYTE,
        /* data     = */ data);

    if (mipmaps)
    {
        if (glGenerateMipmap != nullptr)
        {
            glGenerateMipmap(texTarget);
        }
        else
        {
            mipmaps = false;
        }
    }

    CHECK_GL_ERRORS(&app);

    GLenum glMinFilter = 0;
    GLenum glMagFilter = 0;
    GLenum glWrapMode  = 0;

    switch (texFilter)
    {
    case Filter::Nearest :
        glMinFilter = glMagFilter = GL_NEAREST;
        break;

    case Filter::Linear :
        glMinFilter = glMagFilter = GL_LINEAR;
        break;

    case Filter::LinearMipmaps :
        glMinFilter = GL_LINEAR_MIPMAP_LINEAR;
        glMagFilter = GL_LINEAR;
        break;

    default :
        app.errorF("Corrupted Texture::Filter enum value!");
    } // switch (texFilter)

    switch (texWrap)
    {
    case WrapMode::Repeat :
        glWrapMode = GL_REPEAT;
        break;

    case WrapMode::Clamp :
        glWrapMode = GL_CLAMP_TO_EDGE;
        break;

    default :
        app.errorF("Corrupted Texture::WrapMode enum value!");
    } // switch (texWrap)

    glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, glMinFilter);
    glTexParameteri(texTarget, GL_TEXTURE_MAG_FILTER, glMagFilter);
    glTexParameteri(texTarget, GL_TEXTURE_WRAP_S, glWrapMode);
    glTexParameteri(texTarget, GL_TEXTURE_WRAP_T, glWrapMode);
    glTexParameteri(texTarget, GL_TEXTURE_WRAP_R, glWrapMode);

    CHECK_GL_ERRORS(&app);

    // Save them into the members:
    handle     = glTexHandle;
    target     = texTarget;
    tmu        = texUnit;
    width      = w;
    height     = h;
    filter     = texFilter;
    wrapMode   = texWrap;
    hasMipmaps = mipmaps;
}

void GLTexture::initWithCheckerPattern(const int numSquares,
                                       const float (*colors)[4],
                                       const Filter texFilter,
                                       const int texUnit)
{
    assert((numSquares % 2) == 0);

    struct Rgba
    {
        std::uint8_t r, g, b, a;
    };

    const Rgba defaultColors[2]{
        { 255, 100, 255, 255 }, // pink
        { 0,   0,   0,   255 }  // black
    };

    Rgba byteColors[2];
    if (colors != nullptr)
    {
        for (int i = 0; i < 2; ++i)
        {
            byteColors[i].r = static_cast<std::uint8_t>(colors[i][0] * 255.0f);
            byteColors[i].g = static_cast<std::uint8_t>(colors[i][1] * 255.0f);
            byteColors[i].b = static_cast<std::uint8_t>(colors[i][2] * 255.0f);
            byteColors[i].a = static_cast<std::uint8_t>(colors[i][3] * 255.0f);
        }
    }
    else
    {
        byteColors[0] = defaultColors[0];
        byteColors[1] = defaultColors[1];
    }

    const int checkerDim  = 64;
    const int checkerSize = checkerDim / numSquares;

    int x, y, colorindex;
    Rgba buffer[checkerDim * checkerDim];

    for (y = 0; y < checkerDim; ++y)
    {
        for (x = 0; x < checkerDim; ++x)
        {
            colorindex = ((y / checkerSize) + (x / checkerSize)) % 2;
            buffer[x + (y * checkerDim)] = byteColors[colorindex];
        }
    }

    initFromData(reinterpret_cast<std::uint8_t *>(buffer),
                 checkerDim, checkerDim, 4, texFilter,
                 WrapMode::Clamp, true, texUnit, GL_TEXTURE_2D);
}

void GLTexture::cleanup() noexcept
{
    if (isInitialized())
    {
        glActiveTexture(GL_TEXTURE0 + tmu);
        glBindTexture(target, 0); // Clean the binding point, to be sure.

        glDeleteTextures(1, &handle);
        width = height = 0;
        handle = 0;
    }
}

void GLTexture::bind() const noexcept
{
    if (!isInitialized())
    {
        app.printF("Trying to bind an invalid texture!");
    }
    glActiveTexture(GL_TEXTURE0 + tmu);
    glBindTexture(target, handle);
}

void GLTexture::bindNull(const int texUnit, const GLenum texTarget) noexcept
{
    glActiveTexture(GL_TEXTURE0 + texUnit);
    glBindTexture(texTarget, 0);
}

// ========================================================
// class GLShaderProg:
// ========================================================

std::string GLShaderProg::glslVersionDirective{};

GLShaderProg::GLShaderProg(GLFWApp & owner)
    : app{ owner }
    , handle{ 0 }
{
    // Leave uninitialized.
}

GLShaderProg::GLShaderProg(GLFWApp & owner,
                           const std::string & vsFile,
                           const std::string & fsFile)
    : GLShaderProg{ owner }
{
    initFromFiles(vsFile, fsFile);
}

GLShaderProg::~GLShaderProg()
{
    cleanup();
}

void GLShaderProg::initFromFiles(const std::string & vsFile,
                                 const std::string & fsFile)
{
    assert(!vsFile.empty());
    assert(!fsFile.empty());

    if (isInitialized())
    {
        app.errorF("Shader program already initialized! Call cleanup() first!");
    }

    // Queried once and stored for the subsequent shader loads.
    // This ensures we use the best version available.
    if (glslVersionDirective.empty())
    {
        int  slMajor    = 0;
        int  slMinor    = 0;
        int  versionNum = 0;
        auto versionStr = reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION));

        if (std::sscanf(versionStr, "%d.%d", &slMajor, &slMinor) == 2)
        {
            versionNum = (slMajor * 100) + slMinor;
        }
        else
        {
            // Fall back to the lowest acceptable version.
            // Assume #version 150 -> OpenGL 3.2
            versionNum = 150;
        }
        glslVersionDirective = "#version " + std::to_string(versionNum) + "\n";
    }

    const auto vsSrc = loadShaderFile(vsFile.c_str());
    const auto fsSrc = loadShaderFile(fsFile.c_str());
    if (vsSrc == nullptr || fsSrc == nullptr)
    {
        app.errorF("Failed to load one or more shader files!");
    }

    const auto glProgHandle = glCreateProgram();
    if (glProgHandle == 0)
    {
        app.errorF("Failed to allocate a new GL program handle! Possibly out-of-memory!");
    }

    const auto glVsHandle = glCreateShader(GL_VERTEX_SHADER);
    if (glVsHandle == 0)
    {
        app.errorF("Failed to allocate a new GL shader handle! Possibly out-of-memory!");
    }

    const auto glFsHandle = glCreateShader(GL_FRAGMENT_SHADER);
    if (glFsHandle == 0)
    {
        app.errorF("Failed to allocate a new GL shader handle! Possibly out-of-memory!");
    }

    // Vertex shader:
    const char * vsSrcStrings[]{ glslVersionDirective.c_str(), vsSrc.get() };
    glShaderSource(glVsHandle, 2, vsSrcStrings, nullptr);
    glCompileShader(glVsHandle);
    glAttachShader(glProgHandle, glVsHandle);

    // Fragment shader:
    const char * fsSrcStrings[]{ glslVersionDirective.c_str(), fsSrc.get() };
    glShaderSource(glFsHandle, 2, fsSrcStrings, nullptr);
    glCompileShader(glFsHandle);
    glAttachShader(glProgHandle, glFsHandle);

    // Link the Shader Program then check and print the info logs, if any.
    glLinkProgram(glProgHandle);
    checkShaderInfoLogs(glProgHandle, glVsHandle, glFsHandle);

    // After a program is linked the shader objects can be safely detached and deleted.
    // Also recommended to save on the memory that would be wasted by keeping the shaders alive.
    glDetachShader(glProgHandle, glVsHandle);
    glDetachShader(glProgHandle, glFsHandle);
    glDeleteShader(glVsHandle);
    glDeleteShader(glFsHandle);

    // OpenGL likes to defer GPU resource allocation to the first time
    // an object is bound to the current state. Binding it know should
    // "warm up" the resource and avoid lag on the first frame rendered with it.
    glUseProgram(glProgHandle);

    CHECK_GL_ERRORS(&app);
    handle = glProgHandle;

    app.printF("New shader program created from \"%s\" and \"%s\".",
               vsFile.c_str(), fsFile.c_str());
}

void GLShaderProg::cleanup() noexcept
{
    if (isInitialized())
    {
        glUseProgram(0);
        glDeleteProgram(handle);
        handle = 0;
    }
}

void GLShaderProg::bind() const noexcept
{
    if (!isInitialized())
    {
        app.printF("Trying to bind an invalid shader program!");
    }
    glUseProgram(handle);
}

void GLShaderProg::bindNull() noexcept
{
    glUseProgram(0);
}

void GLShaderProg::checkShaderInfoLogs(const GLuint progHandle,
                                       const GLuint vsHandle,
                                       const GLuint fsHandle) const
{
    constexpr int InfoLogMaxChars = 2048;

    GLsizei charsWritten;
    GLchar infoLogBuf[InfoLogMaxChars];

    charsWritten = 0;
    std::memset(infoLogBuf, 0, sizeof(infoLogBuf));
    glGetProgramInfoLog(progHandle, InfoLogMaxChars - 1, &charsWritten, infoLogBuf);
    if (charsWritten > 0)
    {
        app.printF("------ GL PROGRAM INFO LOG ----------");
        app.printF("%s", infoLogBuf);
    }

    charsWritten = 0;
    std::memset(infoLogBuf, 0, sizeof(infoLogBuf));
    glGetShaderInfoLog(vsHandle, InfoLogMaxChars - 1, &charsWritten, infoLogBuf);
    if (charsWritten > 0)
    {
        app.printF("------ GL VERT SHADER INFO LOG ------");
        app.printF("%s", infoLogBuf);
    }

    charsWritten = 0;
    std::memset(infoLogBuf, 0, sizeof(infoLogBuf));
    glGetShaderInfoLog(fsHandle, InfoLogMaxChars - 1, &charsWritten, infoLogBuf);
    if (charsWritten > 0)
    {
        app.printF("------ GL FRAG SHADER INFO LOG ------");
        app.printF("%s", infoLogBuf);
    }

    GLint linkStatus = GL_FALSE;
    glGetProgramiv(progHandle, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE)
    {
        app.printF("Warning! Failed to link GL shader program.");
    }
}

std::unique_ptr<char[]> GLShaderProg::loadShaderFile(const char * filename) const
{
    assert(filename != nullptr && *filename != '\0');

    FILE * fileIn = std::fopen(filename, "rb");
    if (fileIn == nullptr)
    {
        app.printF("Can't open shader file \"%s\"!", filename);
        return nullptr;
    }

    std::fseek(fileIn, 0, SEEK_END);
    const auto fileLength = std::ftell(fileIn);
    std::fseek(fileIn, 0, SEEK_SET);

    if (fileLength <= 0 || std::ferror(fileIn))
    {
        app.printF("Error getting length or empty shader file! \"%s\".", filename);
        std::fclose(fileIn);
        return nullptr;
    }

    std::unique_ptr<char[]> fileContents{ new char[fileLength + 1] };
    if (std::fread(fileContents.get(), 1, fileLength, fileIn) != std::size_t(fileLength))
    {
        app.printF("Failed to read whole shader file! \"%s\".", filename);
        std::fclose(fileIn);
        return nullptr;
    }

    std::fclose(fileIn);
    fileContents[fileLength] = '\0';

    return fileContents;
}

GLint GLShaderProg::getUniformLocation(const std::string & uniformName) const noexcept
{
    if (uniformName.empty() || handle == 0)
    {
        return -1;
    }
    return glGetUniformLocation(handle, uniformName.c_str());
}

void GLShaderProg::setUniform1i(const GLint loc, const int val) noexcept
{
    if (loc < 0)
    {
        app.printF("setUniform1i: Invalid uniform location %d", loc);
        return;
    }
    glUniform1i(loc, val);
}

void GLShaderProg::setUniform1f(const GLint loc, const float val) noexcept
{
    if (loc < 0)
    {
        app.printF("setUniform1f: Invalid uniform location %d", loc);
        return;
    }
    glUniform1f(loc, val);
}

void GLShaderProg::setUniformVec3(const GLint loc, const Vec3 & v) noexcept
{
    if (loc < 0)
    {
        app.printF("setUniformVec3: Invalid uniform location %d", loc);
        return;
    }
    glUniform3f(loc, v.getX(), v.getY(), v.getZ());
}

void GLShaderProg::setUniformVec4(const GLint loc, const Vec4 & v) noexcept
{
    if (loc < 0)
    {
        app.printF("setUniformVec4: Invalid uniform location %d", loc);
        return;
    }
    glUniform4f(loc, v.getX(), v.getY(), v.getZ(), v.getW());
}

void GLShaderProg::setUniformMat4(const GLint loc, const Mat4 & m) noexcept
{
    if (loc < 0)
    {
        app.printF("setUniformMat4: Invalid uniform location %d", loc);
        return;
    }
    glUniformMatrix4fv(loc, 1, GL_FALSE, toFloatPtr(m));
}

void GLShaderProg::setUniformPoint3(GLint loc, const Point3 & v) noexcept
{
    if (loc < 0)
    {
        app.printF("setUniformPoint3: Invalid uniform location %d", loc);
        return;
    }
    glUniform3f(loc, v.getX(), v.getY(), v.getZ());
}

// ========================================================
// class GLVertexArray:
// ========================================================

GLVertexArray::GLVertexArray(GLFWApp & owner)
    : app        { owner }
    , vaHandle   { 0 }
    , vbHandle   { 0 }
    , ibHandle   { 0 }
    , dataUsage  { 0 }
    , vertexCount{ 0 }
    , indexCount { 0 }
{
}

GLVertexArray::GLVertexArray(GLFWApp & owner, const GLDrawVertex * verts, const int vertCount,
                             const GLDrawIndex * indexes, const int idxCount, const GLenum usage,
                             const GLVertexLayout vertLayout)
    : GLVertexArray{ owner }
{
    initFromData(verts, vertCount, indexes, idxCount, usage, vertLayout);
}

GLVertexArray::~GLVertexArray()
{
    cleanup();
}

void GLVertexArray::initFromData(const GLDrawVertex * verts, const int vertCount,
                                 const GLDrawIndex * indexes, const int idxCount,
                                 const GLenum usage, const GLVertexLayout vertLayout)
{
    if (isInitialized())
    {
        app.errorF("Vertex Array already initialized! Call cleanup() first!");
    }

    glGenVertexArrays(1, &vaHandle);
    glBindVertexArray(vaHandle);

    glGenBuffers(1, &vbHandle);
    glBindBuffer(GL_ARRAY_BUFFER, vbHandle);

    // Allow to initialize a VBO with undefined data (verts == nullptr)
    if (vertCount > 0)
    {
        glBufferData(GL_ARRAY_BUFFER, vertCount * sizeof(GLDrawVertex), verts, usage);
    }

    // Index buffer is optional. Passing null or 0 size disables it.
    if (indexes != nullptr && idxCount > 0)
    {
        glGenBuffers(1, &ibHandle);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibHandle);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxCount * sizeof(GLDrawIndex), indexes, usage);
    }
    else
    {
        ibHandle = 0;
    }

    CHECK_GL_ERRORS(&app);

    switch (vertLayout)
    {
    case GLVertexLayout::Triangles :
        setGLTrianglesVertexLayout();
        break;

    case GLVertexLayout::Lines :
        setGLLinesVertexLayout();
        break;

    case GLVertexLayout::Points :
        setGLPointsVertexLayout();
        break;

    default :
        app.errorF("Invalid GLVertexLayout enum!");
    } // switch (vertLayout)

    // Save these for later:
    dataUsage   = usage;
    vertexCount = vertCount;
    indexCount  = idxCount;

    // VAOs can be a pain in the neck if left enabled...
    bindNull();

    app.printF("New vertex array created: %d verts, %d indexes.", vertexCount, indexCount);
}

void GLVertexArray::cleanup() noexcept
{
    bindNull();
    if (vaHandle != 0)
    {
        glDeleteVertexArrays(1, &vaHandle);
        vaHandle = 0;
    }
    if (vbHandle != 0)
    {
        glDeleteBuffers(1, &vbHandle);
        vbHandle = 0;
    }
    if (ibHandle != 0)
    {
        glDeleteBuffers(1, &ibHandle);
        ibHandle = 0;
    }
}

void GLVertexArray::bindVA() const noexcept
{
    if (vaHandle == 0)
    {
        app.printF("Trying to bind a null VAO!");
    }
    glBindVertexArray(vaHandle);
}

void GLVertexArray::bindVB() const noexcept
{
    if (vbHandle == 0)
    {
        app.printF("Trying to bind a null VBO!");
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbHandle);
}

void GLVertexArray::bindIB() const noexcept
{
    if (ibHandle == 0)
    {
        app.printF("Trying to bind a null IBO!");
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibHandle);
}

void GLVertexArray::bindNull() noexcept
{
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GLVertexArray::setGLTrianglesVertexLayout() noexcept
{
    std::size_t offset = 0;

    // Position:
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        /* index     = */ 0,
        /* size      = */ 3,
        /* type      = */ GL_FLOAT,
        /* normalize = */ GL_FALSE,
        /* stride    = */ sizeof(GLDrawVertex),
        /* offset    = */ reinterpret_cast<GLvoid *>(offset));
    offset += sizeof(float) * 3;

    // Normal vector:
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        /* index     = */ 1,
        /* size      = */ 3,
        /* type      = */ GL_FLOAT,
        /* normalize = */ GL_FALSE,
        /* stride    = */ sizeof(GLDrawVertex),
        /* offset    = */ reinterpret_cast<GLvoid *>(offset));
    offset += sizeof(float) * 3;

    // RGBA color:
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        /* index     = */ 2,
        /* size      = */ 4,
        /* type      = */ GL_FLOAT,
        /* normalize = */ GL_FALSE,
        /* stride    = */ sizeof(GLDrawVertex),
        /* offset    = */ reinterpret_cast<GLvoid *>(offset));
    offset += sizeof(float) * 4;

    // Texture coords:
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(
        /* index     = */ 3,
        /* size      = */ 2,
        /* type      = */ GL_FLOAT,
        /* normalize = */ GL_FALSE,
        /* stride    = */ sizeof(GLDrawVertex),
        /* offset    = */ reinterpret_cast<GLvoid *>(offset));
    offset += sizeof(float) * 2;

    // Tangent vector:
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(
        /* index     = */ 4,
        /* size      = */ 3,
        /* type      = */ GL_FLOAT,
        /* normalize = */ GL_FALSE,
        /* stride    = */ sizeof(GLDrawVertex),
        /* offset    = */ reinterpret_cast<GLvoid *>(offset));
    offset += sizeof(float) * 3;

    // Bi-tangent vector:
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(
        /* index     = */ 5,
        /* size      = */ 3,
        /* type      = */ GL_FLOAT,
        /* normalize = */ GL_FALSE,
        /* stride    = */ sizeof(GLDrawVertex),
        /* offset    = */ reinterpret_cast<GLvoid *>(offset));
    /*offset += sizeof(float) * 3;*/

    CHECK_GL_ERRORS(&app);
}

void GLVertexArray::setGLLinesVertexLayout() noexcept
{
    std::size_t offset = 0;

    // Position:
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        /* index     = */ 0,
        /* size      = */ 3,
        /* type      = */ GL_FLOAT,
        /* normalize = */ GL_FALSE,
        /* stride    = */ sizeof(GLLineVertex),
        /* offset    = */ reinterpret_cast<GLvoid *>(offset));
    offset += sizeof(float) * 3;

    // RGBA color:
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        /* index     = */ 1,
        /* size      = */ 4,
        /* type      = */ GL_FLOAT,
        /* normalize = */ GL_FALSE,
        /* stride    = */ sizeof(GLLineVertex),
        /* offset    = */ reinterpret_cast<GLvoid *>(offset));
    /*offset += sizeof(float) * 4;*/

    CHECK_GL_ERRORS(&app);
}

void GLVertexArray::setGLPointsVertexLayout() noexcept
{
    std::size_t offset = 0;

    // Position + point size:
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        /* index     = */ 0,
        /* size      = */ 4,
        /* type      = */ GL_FLOAT,
        /* normalize = */ GL_FALSE,
        /* stride    = */ sizeof(GLPointVertex),
        /* offset    = */ reinterpret_cast<GLvoid *>(offset));
    offset += sizeof(float) * 4;

    // RGBA color:
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        /* index     = */ 1,
        /* size      = */ 4,
        /* type      = */ GL_FLOAT,
        /* normalize = */ GL_FALSE,
        /* stride    = */ sizeof(GLPointVertex),
        /* offset    = */ reinterpret_cast<GLvoid *>(offset));
    /*offset += sizeof(float) * 4;*/

    CHECK_GL_ERRORS(&app);
}

void GLVertexArray::updateRawData(const void * vertData, const int vertCount, const int vertSizeBytes,
                                  const void * idxData,  const int idxCount,  const int idxSizeBytes)
{
    assert(isInitialized());

    if (vertData != nullptr && vertCount > 0)
    {
        assert(vbHandle != 0);
        assert(vertSizeBytes > 0);

        glBufferData(GL_ARRAY_BUFFER, vertCount * vertSizeBytes, vertData, dataUsage);
        vertexCount = vertCount;
    }

    if (idxData != nullptr && idxCount > 0)
    {
        assert(ibHandle != 0);
        assert(idxSizeBytes > 0);

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxCount * idxSizeBytes, idxData, dataUsage);
        indexCount = idxCount;
    }
}

void * GLVertexArray::mapVB(const GLenum access) noexcept
{
    if (vbHandle == 0)
    {
        app.printF("Trying to map a null VBO!");
    }
    return glMapBuffer(GL_ARRAY_BUFFER, access);
}

void * GLVertexArray::mapVBRange(const int offsetInBytes,
                                 const int sizeInBytes,
                                 const GLbitfield access) noexcept
{
    if (vbHandle == 0)
    {
        app.printF("Trying to map a null VBO!");
    }
    return glMapBufferRange(GL_ARRAY_BUFFER, offsetInBytes, sizeInBytes, access);
}

void GLVertexArray::unMapVB() noexcept
{
    if (!glUnmapBuffer(GL_ARRAY_BUFFER))
    {
        app.printF("glUnmapBuffer(GL_ARRAY_BUFFER) failed for GL VBO %d!", vbHandle);
        CHECK_GL_ERRORS(&app);
    }
}

void * GLVertexArray::mapIB(const GLenum access) noexcept
{
    if (ibHandle == 0)
    {
        app.printF("Trying to map a null IBO!");
    }
    return glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, access);
}

void * GLVertexArray::mapIBRange(const int offsetInBytes,
                                 const int sizeInBytes,
                                 const GLbitfield access) noexcept
{
    if (ibHandle == 0)
    {
        app.printF("Trying to map a null IBO!");
    }
    return glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, offsetInBytes, sizeInBytes, access);
}

void GLVertexArray::unMapIB() noexcept
{
    if (!glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER))
    {
        app.printF("glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER) failed for GL IBO %d!", ibHandle);
        CHECK_GL_ERRORS(&app);
    }
}

void GLVertexArray::draw(const GLenum renderMode) const noexcept
{
    if (isIndexed())
    {
        drawIndexed(renderMode, 0, indexCount);
    }
    else
    {
        drawUnindexed(renderMode, 0, vertexCount);
    }
}

void GLVertexArray::drawIndexed(const GLenum renderMode,
                                const int firstIndex,
                                const int idxCount) const noexcept
{
    assert(isInitialized());
    assert(isIndexed());

    assert(idxCount > 0);
    assert(idxCount <= indexCount);

    const std::uintptr_t offsetBytes = firstIndex * sizeof(GLDrawIndex);
    glDrawElements(renderMode, idxCount, GLDrawIndexType,
                   reinterpret_cast<const GLvoid *>(offsetBytes));
}

void GLVertexArray::drawUnindexed(const GLenum renderMode,
                                  const int firstVertex,
                                  const int vertCount) const noexcept
{
    assert(isInitialized());

    assert(vertCount > 0);
    assert(vertCount <= vertexCount);

    glDrawArrays(renderMode, firstVertex, vertCount);
}

void GLVertexArray::drawIndexedBaseVertex(const GLenum renderMode,
                                          const int firstIndex,
                                          const int idxCount,
                                          const int baseVert) const noexcept
{
    assert(isInitialized());
    assert(isIndexed());

    assert(idxCount > 0);
    assert(idxCount <= indexCount);

    const std::uintptr_t offsetBytes = firstIndex * sizeof(GLDrawIndex);
    glDrawElementsBaseVertex(renderMode, idxCount, GLDrawIndexType,
                             reinterpret_cast<const GLvoid *>(offsetBytes), baseVert);
}

// ========================================================
// class GLBatchLineRenderer:
// ========================================================

GLBatchLineRenderer::GLBatchLineRenderer(GLFWApp & owner, const int initialBatchSizeInLines)
    : linesShader    { owner }
    , linesVA        { owner }
    , linesMvpMatrix { Mat4::identity() }
    , needGLUpdate   { false }
{
    const int vertCount = initialBatchSizeInLines * 2; // 2 verts per line.
    if (vertCount > 0)
    {
        lineVerts.reserve(vertCount);
    }

    // Shader setup:
    linesShader.initFromFiles("source/shaders/lines.vert", "source/shaders/lines.frag");
    linesMvpMatrixLocation = linesShader.getUniformLocation("u_MvpMatrix");

    // VBO setup:
    linesVA.initFromData(nullptr, 0, nullptr, 0, GL_DYNAMIC_DRAW, GLVertexLayout::Lines);
}

void GLBatchLineRenderer::addLine(const Point3 & from, const Point3 & to,
                                  const Vec4 & color)
{
    lineVerts.emplace_back(from, color);
    lineVerts.emplace_back(to, color);
    needGLUpdate = true;
}

void GLBatchLineRenderer::addLine(const Point3 & from, const Point3 & to,
                                  const Vec4 & fromColor, const Vec4 & toColor)
{
    lineVerts.emplace_back(from, fromColor);
    lineVerts.emplace_back(to, toColor);
    needGLUpdate = true;
}

void GLBatchLineRenderer::drawLines()
{
    if (lineVerts.empty())
    {
        return;
    }

    linesVA.bindVA();
    if (needGLUpdate)
    {
        linesVA.bindVB();
        linesVA.updateRawData(lineVerts.data(), lineVerts.size(), sizeof(GLLineVertex), nullptr, 0, 0);
        needGLUpdate = false;
    }

    linesShader.bind();
    linesShader.setUniformMat4(linesMvpMatrixLocation, linesMvpMatrix);

    linesVA.draw(GL_LINES);
    linesVA.bindNull();
}

void GLBatchLineRenderer::clear()
{
    if (lineVerts.empty())
    {
        return;
    }

    lineVerts.clear();
    needGLUpdate = false;
}

// ========================================================
// class GLBatchPointRenderer:
// ========================================================

GLBatchPointRenderer::GLBatchPointRenderer(GLFWApp & owner, const int initialBatchSizeInPoints)
    : pointsShader    { owner }
    , pointsVA        { owner }
    , pointsMvpMatrix { Mat4::identity() }
    , needGLUpdate    { false }
{
    if (initialBatchSizeInPoints > 0)
    {
        pointVerts.reserve(initialBatchSizeInPoints);
    }

    // This has to be enabled since the point drawing shader will use gl_PointSize.
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Shader setup:
    pointsShader.initFromFiles("source/shaders/points.vert", "source/shaders/points.frag");
    pointsMvpMatrixLocation = pointsShader.getUniformLocation("u_MvpMatrix");

    // VBO setup:
    pointsVA.initFromData(nullptr, 0, nullptr, 0, GL_DYNAMIC_DRAW, GLVertexLayout::Points);
}

void GLBatchPointRenderer::addPoint(const Point3 & point, const float size, const Vec4 & color)
{
    pointVerts.emplace_back(point, size, color);
    needGLUpdate = true;
}

void GLBatchPointRenderer::drawPoints()
{
    if (pointVerts.empty())
    {
        return;
    }

    pointsVA.bindVA();
    if (needGLUpdate)
    {
        pointsVA.bindVB();
        pointsVA.updateRawData(pointVerts.data(), pointVerts.size(), sizeof(GLPointVertex), nullptr, 0, 0);
        needGLUpdate = false;
    }

    pointsShader.bind();
    pointsShader.setUniformMat4(pointsMvpMatrixLocation, pointsMvpMatrix);

    pointsVA.draw(GL_POINTS);
    pointsVA.bindNull();
}

void GLBatchPointRenderer::clear()
{
    if (pointVerts.empty())
    {
        return;
    }

    pointVerts.clear();
    needGLUpdate = false;
}

// ========================================================
// GLFW callbacks from gl_main.cpp:
// ========================================================

extern "C"
{
void mousePosCallback(GLFWwindow * window, double xpos, double ypos);
void mouseScrollCallback(GLFWwindow * window, double xoffset, double yoffset);
void mouseButtonCallback(GLFWwindow * window, int button, int action, int mods);
void keyCallback(GLFWwindow * window, int key, int scanCode, int action, int mods);
void keyCharCallback(GLFWwindow * window, unsigned int chr);
}

// Anchor the vtable to this file so it doesn't get
// replicated in every other file including the header.
GLError::~GLError() { }

// ========================================================
// class GLFWApp:
// ========================================================

GLFWApp::GLFWApp(const int winWidth, const int winHeight,
                 const float * clearColor, std::string title)
    : windowWidth   { winWidth  }
    , windowHeight  { winHeight }
    , glfwWindowPtr { nullptr   }
    , windowTitle   { std::move(title) }
{
    if (clearColor != nullptr)
    {
        setClearScrColor(clearColor);
    }
    else
    {
        const float black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        setClearScrColor(black);
    }

    tryCreateWindow();
}

GLFWApp::~GLFWApp()
{
    gl3wShutdown();

    if (glfwWindowPtr != nullptr)
    {
        glfwDestroyWindow(glfwWindowPtr);
        glfwWindowPtr = nullptr;
    }

    glfwTerminate();
}

std::int64_t GLFWApp::getTimeMilliseconds() const noexcept
{
    const double seconds = glfwGetTime();
    return static_cast<std::int64_t>(seconds * 1000.0);
}

void GLFWApp::checkGLErrors(const char * function, const char * filename,
                            const int lineNum, const bool throwExcept)
{
    struct Err
    {
        static const char * toString(const GLenum errorCode) noexcept
        {
            switch (errorCode)
            {
            case GL_NO_ERROR                      : return "GL_NO_ERROR";
            case GL_INVALID_ENUM                  : return "GL_INVALID_ENUM";
            case GL_INVALID_VALUE                 : return "GL_INVALID_VALUE";
            case GL_INVALID_OPERATION             : return "GL_INVALID_OPERATION";
            case GL_INVALID_FRAMEBUFFER_OPERATION : return "GL_INVALID_FRAMEBUFFER_OPERATION";
            case GL_OUT_OF_MEMORY                 : return "GL_OUT_OF_MEMORY";
            case GL_STACK_UNDERFLOW               : return "GL_STACK_UNDERFLOW"; // Legacy; not used on GL3+
            case GL_STACK_OVERFLOW                : return "GL_STACK_OVERFLOW";  // Legacy; not used on GL3+
            default                               : return "Unknown GL error";
            } // switch (errorCode)
        }
    };

    GLint errorCount = 0;
    GLenum errorCode = glGetError();

    while (errorCode != GL_NO_ERROR)
    {
        printF("OpenGL error %X ( %s ) in %s(), file %s(%d).",
               errorCode, Err::toString(errorCode), function, filename, lineNum);

        errorCode = glGetError();
        errorCount++;
    }

    if (errorCount > 0 && throwExcept)
    {
        errorF("%d OpenGL errors were detected!", errorCount);
    }
}

void GLFWApp::printF(const char * format, ...)
{
    va_list vaArgs;
    char buffer[4096];

    va_start(vaArgs, format);
    std::vsnprintf(buffer, arrayLength(buffer), format, vaArgs);
    va_end(vaArgs);

    std::cout << buffer << "\n";
}

void GLFWApp::errorF(const char * format, ...)
{
    va_list vaArgs;
    char buffer[4096];

    va_start(vaArgs, format);
    std::vsnprintf(buffer, arrayLength(buffer), format, vaArgs);
    va_end(vaArgs);

    std::cerr << "ERROR! " << buffer << "\n";
    throw GLError(buffer);
}

void GLFWApp::grabSystemCursor()
{
    assert(glfwWindowPtr != nullptr);
    glfwSetInputMode(glfwWindowPtr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void GLFWApp::restoreSystemCursor()
{
    assert(glfwWindowPtr != nullptr);
    glfwSetInputMode(glfwWindowPtr, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void GLFWApp::setWindowTitle(std::string title) noexcept
{
    assert(glfwWindowPtr != nullptr);
    windowTitle = std::move(title);
    glfwSetWindowTitle(glfwWindowPtr, windowTitle.c_str());
}

void GLFWApp::setClearScrColor(const float color[4]) noexcept
{
    for (int i = 0; i < arrayLength(clearScrColor); ++i)
    {
        clearScrColor[i] = color[i];
    }
}

void GLFWApp::tryCreateWindow()
{
    if (windowWidth <= 0 || windowHeight <= 0)
    {
        errorF("Bad window dimensions! %d, %d", windowWidth, windowHeight);
    }

    if (glfwWindowPtr != nullptr)
    {
        errorF("Window already created!");
    }

    if (!glfwInit())
    {
        errorF("glfwInit() failed!");
    }

    glfwWindowHint(GLFW_RESIZABLE,  false);
    glfwWindowHint(GLFW_DEPTH_BITS, 32);

    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    if (windowTitle.empty())
    {
        windowTitle = "OpenGL Window";
    }

    glfwWindowPtr = glfwCreateWindow(windowWidth, windowHeight,
                                     windowTitle.c_str(), nullptr, nullptr);

    if (glfwWindowPtr == nullptr)
    {
        errorF("Unable to create GLFW window!");
    }

    // GLFW input callbacks:
    glfwSetCursorPosCallback(glfwWindowPtr,   &mousePosCallback);
    glfwSetScrollCallback(glfwWindowPtr,      &mouseScrollCallback);
    glfwSetMouseButtonCallback(glfwWindowPtr, &mouseButtonCallback);
    glfwSetKeyCallback(glfwWindowPtr,         &keyCallback);
    glfwSetCharCallback(glfwWindowPtr,        &keyCharCallback);

    // Make the drawing context (OpenGL) current for this thread:
    glfwMakeContextCurrent(glfwWindowPtr);

    // GL3W initialization:
    if (!gl3wInit())
    {
        errorF("gl3wInit() failed!");
    }

    // Default OpenGL states:
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    glClearColor(clearScrColor[0], clearScrColor[1],
                 clearScrColor[2], clearScrColor[3]);
}

void GLFWApp::runMainLoop()
{
    assert(glfwWindowPtr != nullptr);

    std::int64_t t0, t1;
    std::int64_t deltaTime = 33; // Assume an initial ~30fps.

    while (!glfwWindowShouldClose(glfwWindowPtr))
    {
        t0 = getTimeMilliseconds();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        onFrameUpdate(t0, deltaTime);
        onFrameRender(t0, deltaTime);

        glfwSwapBuffers(glfwWindowPtr);
        glfwPollEvents();

        t1 = getTimeMilliseconds();
        deltaTime = t1 - t0;
    }
}

void GLFWApp::onInit()
{
    printF("---- GLFWApp::onInit ----");
    // Default impl is a no-op.
}

void GLFWApp::onShutdown()
{
    printF("---- GLFWApp::onShutdown ----");
    // Default impl is a no-op.
}

void GLFWApp::onFrameUpdate(std::int64_t /* currentTimeMillis */,
                            std::int64_t /* elapsedTimeMillis */)
{
    // Default impl is a no-op.
}

void GLFWApp::onFrameRender(std::int64_t /* currentTimeMillis */,
                            std::int64_t /* elapsedTimeMillis */)
{
    // Default impl is a no-op.
}

void GLFWApp::onMouseMotion(int /* x */, int /* y */)
{
    // Default impl is a no-op.
}

void GLFWApp::onMouseScroll(double /* xOffset */, double /* yOffset */)
{
    // Default impl is a no-op.
}

void GLFWApp::onMouseButton(MouseButton /* button */, bool /* pressed */)
{
    // Default impl is a no-op.
}

void GLFWApp::onKey(int /* key */, int /* action */, int /* mods */)
{
    // Default impl is a no-op.
}

void GLFWApp::onKeyChar(unsigned int /* chr */)
{
    // Default impl is a no-op.
}

// ========================================================
// Pseudo-random number generators:
// ========================================================

unsigned int randomInt()
{
    //
    // Initial XOR-Shift constants presented in this paper:
    //  http://www.jstatsoft.org/v08/i14/paper
    //
    static unsigned int x = 123456789u;
    static unsigned int y = 362436069u;
    static unsigned int z = 521288629u;
    static unsigned int w = 88675123u;

    // XOR-Shift-128 RNG:
    unsigned int t = x ^ (x << 11);
    x = y;
    y = z;
    z = w;
    w = w ^ (w >> 19) ^ t ^ (t >> 8);
    return w;
}

float randomFloat()
{
    return static_cast<float>(randomInt()) / UINT_MAX;
}

int randomInt(const int lowerBound, const int upperBound)
{
    if (lowerBound == upperBound)
    {
        return lowerBound;
    }
    return (randomInt() % (upperBound - lowerBound)) + lowerBound;
}

float randomFloat(const float lowerBound, const float upperBound)
{
    return (randomFloat() * (upperBound - lowerBound)) + lowerBound;
}

// ========================================================
// deriveNormalsAndTangents():
//
// Derives the normal and orthogonal tangent vectors for the triangle vertexes.
// For each vertex the normal and tangent vectors are derived from all triangles
// using the vertex which results in smooth tangents across the mesh.
//
// This code was adapted directly from DOOM3's renderer. Original source
// can be found at: https://github.com/id-Software/DOOM-3-BFG/blob/master/neo/renderer/tr_trisurf.cpp#L908
// ========================================================

// This should keep us within strict aliasing legality.
union Float2Int
{
    float asFloat;
    std::uint32_t asU32;
};

static float invLength(const Vec3 & v)
{
    return 1.0f / std::sqrt((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
}

static std::uint32_t floatSignBit(float f)
{
    Float2Int f2i;
    f2i.asFloat = f;
    return (f2i.asU32 & (1 << 31));
}

static float toggleSignBit(float f, std::uint32_t signBit)
{
    Float2Int f2i;
    f2i.asFloat = f;
    f2i.asU32 ^= signBit;
    return f2i.asFloat;
}

void deriveNormalsAndTangents(const GLDrawVertex * vertsIn,   const int vertCount,
                              const GLDrawIndex  * indexesIn, const int indexCount,
                              GLDrawVertex * vertsOut)
{
    assert(vertsIn   != nullptr);
    assert(indexesIn != nullptr);
    assert(vertsOut  != nullptr);
    assert(vertCount  > 0);
    assert(indexCount > 0);

    const Vec3 vZero{ 0.0f, 0.0f, 0.0f };
    std::vector<Vec3> vertexNormals(vertCount, vZero);
    std::vector<Vec3> vertexTangents(vertCount, vZero);
    std::vector<Vec3> vertexBitangents(vertCount, vZero);

    for (int i = 0; i < indexCount; i += 3)
    {
        const int v0 = indexesIn[i + 0];
        const int v1 = indexesIn[i + 1];
        const int v2 = indexesIn[i + 2];

        const GLDrawVertex & a = vertsIn[v0];
        const GLDrawVertex & b = vertsIn[v1];
        const GLDrawVertex & c = vertsIn[v2];

        const float aUV[2]{ a.u, a.v };
        const float bUV[2]{ b.u, b.v };
        const float cUV[2]{ c.u, c.v };

        const float d0[5]{
            b.px - a.px,
            b.py - a.py,
            b.pz - a.pz,
            bUV[0] - aUV[0],
            bUV[1] - aUV[1] };

        const float d1[5]{
            c.px - a.px,
            c.py - a.py,
            c.pz - a.pz,
            cUV[0] - aUV[0],
            cUV[1] - aUV[1] };

        Vec3 normal{ (d1[1] * d0[2]) - (d1[2] * d0[1]),
                     (d1[2] * d0[0]) - (d1[0] * d0[2]),
                     (d1[0] * d0[1]) - (d1[1] * d0[0]) };

        const float f0 = invLength(normal);
        normal *= f0;

        // NOTE: Had to flip the normal direction here! This was not
        // in the original DOOM3 code. Is this necessary because they
        // use a different texture and axis setup? Not sure...
        normal *= -1.0f;

        // Area sign bit:
        const float area = (d0[3] * d1[4]) - (d0[4] * d1[3]);
        const std::uint32_t signBit = floatSignBit(area);

        Vec3 tangent{ (d0[0] * d1[4]) - (d0[4] * d1[0]),
                      (d0[1] * d1[4]) - (d0[4] * d1[1]),
                      (d0[2] * d1[4]) - (d0[4] * d1[2]) };

        float f1 = invLength(tangent);
        f1 = toggleSignBit(f1, signBit);
        tangent *= f1;

        Vec3 bitangent{ (d0[3] * d1[0]) - (d0[0] * d1[3]),
                        (d0[3] * d1[1]) - (d0[1] * d1[3]),
                        (d0[3] * d1[2]) - (d0[2] * d1[3]) };

        float f2 = invLength(bitangent);
        f2 = toggleSignBit(f2, signBit);
        bitangent *= f2;

        vertexNormals[v0] += normal;
        vertexTangents[v0] += tangent;
        vertexBitangents[v0] += bitangent;

        vertexNormals[v1] += normal;
        vertexTangents[v1] += tangent;
        vertexBitangents[v1] += bitangent;

        vertexNormals[v2] += normal;
        vertexTangents[v2] += tangent;
        vertexBitangents[v2] += bitangent;
    }

    // Project the summed vectors onto the normal plane and normalize.
    // The tangent vectors will not necessarily be orthogonal to each
    // other, but they will be orthogonal to the surface normal.
    for (int i = 0; i < vertCount; ++i)
    {
        const float normalScale = invLength(vertexNormals[i]);
        vertexNormals[i] *= normalScale;

        vertexTangents[i]   -= dot(vertexTangents[i],   vertexNormals[i]) * vertexNormals[i];
        vertexBitangents[i] -= dot(vertexBitangents[i], vertexNormals[i]) * vertexNormals[i];

        const float tangentScale = invLength(vertexTangents[i]);
        vertexTangents[i] *= tangentScale;

        const float bitangentScale = invLength(vertexBitangents[i]);
        vertexBitangents[i] *= bitangentScale;
    }

    // Store the new normals and tangents:
    for (int i = 0; i < vertCount; ++i)
    {
        vertsOut[i].nx = vertexNormals[i][0];
        vertsOut[i].ny = vertexNormals[i][1];
        vertsOut[i].nz = vertexNormals[i][2];

        vertsOut[i].tx = vertexTangents[i][0];
        vertsOut[i].ty = vertexTangents[i][1];
        vertsOut[i].tz = vertexTangents[i][2];

        vertsOut[i].bx = vertexBitangents[i][0];
        vertsOut[i].by = vertexBitangents[i][1];
        vertsOut[i].bz = vertexBitangents[i][2];
    }
}

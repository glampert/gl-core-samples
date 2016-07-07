
// ================================================================================================
// -*- C++ -*-
// File: gl_utils.hpp
// Author: Guilherme R. Lampert
// Created on: 14/11/15
// Brief: Simple OpenGL utilities and helpers.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#ifndef GL_UTILS_HPP
#define GL_UTILS_HPP

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <memory>
#include <string>
#include <vector>

#include "vectormath.hpp"

// ========================================================
// Global macros:
// ========================================================

#if defined(__GNUC__) || defined(__clang__)
    #define ATTR_PRINTF_FUNC(fmtIndex, varIndex) __attribute__((format(printf, fmtIndex, varIndex)))
#else // !GNU && !Clang
    #define ATTR_PRINTF_FUNC(fmtIndex, varIndex) /* unimplemented */
#endif // GNU/Clang

#define CHECK_GL_ERRORS(appPtr) (appPtr)->checkGLErrors(__func__, __FILE__, __LINE__, false)

// ========================================================
// Miscellaneous helpers:
// ========================================================

// Length in elements of type 'T' of statically allocated C++ arrays.
template<class T, int N>
constexpr int arrayLength(const T (&)[N])
{
    return N;
}

// Clamp any value within minimum/maximum range, inclusive.
template<class T>
constexpr T clamp(const T & x, const T & minimum, const T & maximum)
{
    return (x < minimum) ? minimum : (x > maximum) ? maximum : x;
}

// Degrees => Radians conversion:
constexpr float degToRad(const float degrees)
{
    return degrees * (3.1415926535897931f / 180.0f);
}

// Radians => Degrees conversion:
constexpr float radToDeg(const float radians)
{
    return radians * (180.0f / 3.1415926535897931f);
}

// Milliseconds => Seconds conversion:
constexpr double millisToSeconds(const double ms)
{
    return ms * 0.001;
}

// Seconds => Milliseconds conversion:
constexpr double secondsToMillis(const double sec)
{
    return sec * 1000.0;
}

// Divides a width by a height as floating point to
// produce the aspect ratio of a projection matrix.
constexpr float aspectRatio(const float w, const float h)
{
    return w / h;
}

// Forward declared. Defined further down.
class GLFWApp;

// ========================================================
// Pseudo-random number generators:
// ========================================================

unsigned int randomInt(); // Number between [0, UINT_MAX]
float randomFloat();      // Number between [0.0, 1.0]

// Number between lower/upper bounds, inclusive.
int randomInt(int lowerBound, int upperBound);
float randomFloat(float lowerBound, float upperBound);

// ========================================================
// Our basic drawing geometry types:
// ========================================================

struct GLDrawVertex final
{
    float px, py, pz; // Position
    float nx, ny, nz; // Normal vector
    float r, g, b, a; // Vertex RGBA color [0,1] range
    float u, v;       // Texture coordinates
    float tx, ty, tz; // Tangent vector
    float bx, by, bz; // Bi-tangent vector
};

struct GLLineVertex final
{
    GLLineVertex() = default;
    GLLineVertex(const Point3 & p, const Vec4 & c)
        : x{ p[0] }
        , y{ p[1] }
        , z{ p[2] }
        , r{ c[0] }
        , g{ c[1] }
        , b{ c[2] }
        , a{ c[3] }
    { }

    float x, y, z;
    float r, g, b, a;
};

struct GLPointVertex final
{
    GLPointVertex() = default;
    GLPointVertex(const Point3 & p, const float s, const Vec4 & c)
        : x{ p[0] }
        , y{ p[1] }
        , z{ p[2] }
        , size{ s }
        , r{ c[0] }
        , g{ c[1] }
        , b{ c[2] }
        , a{ c[3] }
    { }

    float x, y, z;
    float size;
    float r, g, b, a;
};

// Type used for vertex indexing:
using GLDrawIndex = std::uint16_t;
constexpr GLenum GLDrawIndexType = GL_UNSIGNED_SHORT;

// Supported vertex layouts/formats:
enum class GLVertexLayout
{
    Triangles, // GLDrawVertex layout
    Lines,     // GLLineVertex layout (for GLBatchLineRenderer)
    Points     // GLPointVertex layout (for GLBatchPointRenderer)
};

// Helper to compute model normals, tangents and bi-tangents for normal-mapping.
void deriveNormalsAndTangents(const GLDrawVertex * vertsIn,   int vertCount,
                              const GLDrawIndex  * indexesIn, int indexCount,
                              GLDrawVertex * vertsOut);

// ========================================================
// class GLTexture: Simple OGL texture handle wrapper
// ========================================================

class GLTexture final
{
public:

    // Equivalent to the GL_TEXTURE_FILTER enums
    enum class Filter
    {
        Nearest,
        Linear,
        LinearMipmaps
    };

    // Equivalent to the GL_TEXTURE_WRAP enums
    enum class WrapMode
    {
        Repeat,
        Clamp
    };

    // Copy/assignment is disabled.
    GLTexture(const GLTexture &) = delete;
    GLTexture & operator = (const GLTexture &) = delete;

    // Construct a null/zero texture.
    explicit GLTexture(GLFWApp & owner);

    // Load from image file.
    GLTexture(GLFWApp & owner, const std::string & imageFile,
              bool flipV = false, Filter texFilter = Filter::Nearest,
              WrapMode texWrap = WrapMode::Clamp, bool mipmaps = true,
              int texUnit = 0, GLenum texTarget = GL_TEXTURE_2D);

    // Load from image file. Supports PNG, JPEG, TGA, BMP and GIF.
    void initFromFile(const std::string & imageFile, bool flipV = false,
                      Filter texFilter = Filter::Nearest,
                      WrapMode texWrap = WrapMode::Clamp,
                      bool mipmaps = true, int texUnit = 0,
                      GLenum texTarget = GL_TEXTURE_2D);

    // Setup from RGBA image data. You may free the 'data' pointer after this method returns.
    void initFromData(const std::uint8_t * data, int w, int h, int chans,
                      Filter texFilter = Filter::Nearest,
                      WrapMode texWrap = WrapMode::Clamp,
                      bool mipmaps = true, int texUnit = 0,
                      GLenum texTarget = GL_TEXTURE_2D);

    // Built-in checkerboard texture. Default colors are pink/black if 'colors' is null.
    void initWithCheckerPattern(int numSquares, const float (*colors)[4] = nullptr,
                                Filter texFilter = Filter::Nearest, int texUnit = 0);

    // This frees the underlaying texture handle, but leaves this object intact.
    void cleanup() noexcept;

    // Binds using texture unit and target provided on initialization.
    void bind() const noexcept;

    // Binds zero to the given texture unit and target.
    static void bindNull(int texUnit = 0, GLenum texTarget = GL_TEXTURE_2D) noexcept;

    // Calls cleanup().
    ~GLTexture();

    //
    // Misc accessors:
    //

    GLenum getGLTarget()   const noexcept { return target;   }
    Filter getFilter()     const noexcept { return filter;   }
    WrapMode getWrapMode() const noexcept { return wrapMode; }

    int getWidth()   const noexcept { return width;  }
    int getHeight()  const noexcept { return height; }
    int getTexUnit() const noexcept { return tmu;    }

    bool isMipmapped()   const noexcept { return hasMipmaps;  }
    bool isInitialized() const noexcept { return handle != 0; }

private:

    GLFWApp & app;
    GLuint    handle;
    GLenum    target;
    int       tmu;
    int       width;
    int       height;
    Filter    filter;
    WrapMode  wrapMode;
    bool      hasMipmaps;
};

// ========================================================
// class GLShaderProg: Simple OGL shader program wrapper
// ========================================================

class GLShaderProg final
{
public:

    // Copy/assignment is disabled.
    GLShaderProg(const GLShaderProg &) = delete;
    GLShaderProg & operator = (const GLShaderProg &) = delete;

    // Construct a null/zero shader program.
    explicit GLShaderProg(GLFWApp & owner);

    // Initialize from vertex and fragment shader sources (via initFromFile).
    GLShaderProg(GLFWApp & owner, const std::string & vsFile, const std::string & fsFile);

    // Initialize from vertex and fragment shader sources. Both files must be valid.
    // Prints the shader/program info log to the GLFWApp debug output.
    void initFromFiles(const std::string & vsFile, const std::string & fsFile);

    // This frees the underlaying program handle, but leaves this object intact.
    void cleanup() noexcept;

    // Binds the program object. Will print a warning if the handle is invalid.
    void bind() const noexcept;

    // Binds the null/default program (0).
    static void bindNull() noexcept;

    // True if 'initFromFiles' or the parameterized constructor were called successfully at least once.
    bool isInitialized() const noexcept { return handle != 0; }

    // Calls cleanup().
    ~GLShaderProg();

    // Get the shader uniform handle (AKA location):
    GLint getUniformLocation(const std::string & uniformName) const noexcept;

    // Set uniform values (shader program should be already bound):
    void setUniform1i(GLint loc, int   val) noexcept;
    void setUniform1f(GLint loc, float val) noexcept;
    void setUniformVec3(GLint loc, const Vec3 & v) noexcept;
    void setUniformVec4(GLint loc, const Vec4 & v) noexcept;
    void setUniformMat4(GLint loc, const Mat4 & m) noexcept;
    void setUniformPoint3(GLint loc, const Point3 & v) noexcept;

private:

    void checkShaderInfoLogs(GLuint progHandle, GLuint vsHandle, GLuint fsHandle) const;
    std::unique_ptr<char[]> loadShaderFile(const char * filename) const;

    GLFWApp & app;
    GLuint handle;

    // Shared by all programs. Set when the first shader is loaded.
    static std::string glslVersionDirective;
};

// ========================================================
// class GLVertexArray: OGL Vertex & Index buffers wrapper
// ========================================================

class GLVertexArray final
{
public:

    // Copy/assignment is disabled.
    GLVertexArray(const GLVertexArray &) = delete;
    GLVertexArray & operator = (const GLVertexArray &) = delete;

    // Construct a null/zero/empty VA program.
    explicit GLVertexArray(GLFWApp & owner);

    // Caller provided mesh data. Indexes are optional and may be null.
    GLVertexArray(GLFWApp & owner, const GLDrawVertex * verts, int vertCount,
                  const GLDrawIndex * indexes, int idxCount, GLenum usage,
                  GLVertexLayout vertLayout);

    // Caller provided mesh data. Indexes are optional and may be null.
    void initFromData(const GLDrawVertex * verts, int vertCount,
                      const GLDrawIndex * indexes, int idxCount,
                      GLenum usage, GLVertexLayout vertLayout);

    // Built-ins:
    void initWithBoxMesh(GLenum usage, float width, float height, float depth, const float * color);
    void initWithTeapotMesh(GLenum usage, float scale, const float * color);
    void initWithQuadMesh(GLenum usage, float scale, const float * color);

    // Raw data upload on an already initialized vertex array.
    void updateRawData(const void * vertData, int vertCount, int vertSizeBytes,
                       const void * idxData,  int idxCount,  int idxSizeBytes);

    // This frees the underlaying handles, but leaves this object intact.
    void cleanup() noexcept;

    // Bind the individual buffer for subsequent use:
    void bindVA() const noexcept;
    void bindVB() const noexcept;
    void bindIB() const noexcept;

    // Binds 0 to the VA, VB and IB.
    static void bindNull() noexcept;

    // Sets the vertex layout/format to match GLDrawVertex.
    // 'initFromData()' calls it when successful.
    void setGLTrianglesVertexLayout() noexcept;

    // Auxiliary vertex formats used by the line/point batch renderers.
    void setGLLinesVertexLayout() noexcept;
    void setGLPointsVertexLayout() noexcept;

    // Calls cleanup().
    ~GLVertexArray();

    //
    // Buffer mapping (must bind first):
    //

    void * mapVB(GLenum access) noexcept;
    void * mapVBRange(int offsetInBytes, int sizeInBytes, GLbitfield access) noexcept;
    void unMapVB() noexcept;

    void * mapIB(GLenum access) noexcept;
    void * mapIBRange(int offsetInBytes, int sizeInBytes, GLbitfield access) noexcept;
    void unMapIB() noexcept;

    //
    // Draw calls (must bind first):
    //

    void draw(GLenum renderMode) const noexcept; // => Draws the whole array, indexed or not.
    void drawIndexed(GLenum renderMode, int firstIndex, int idxCount) const noexcept;
    void drawUnindexed(GLenum renderMode, int firstVertex, int vertCount) const noexcept;
    void drawIndexedBaseVertex(GLenum renderMode, int firstIndex, int idxCount, int baseVert) const noexcept;

    //
    // Misc accessors:
    //

    bool isIndexed()     const noexcept { return indexCount > 0; }
    bool isInitialized() const noexcept { return vaHandle  != 0; }

    GLenum getGLUsage()  const noexcept { return dataUsage;   }
    int getIndexCount()  const noexcept { return indexCount;  }
    int getVertexCount() const noexcept { return vertexCount; }

private:

    GLFWApp & app;
    GLuint    vaHandle;
    GLuint    vbHandle;
    GLuint    ibHandle;
    GLenum    dataUsage;
    int       vertexCount;
    int       indexCount;
};

// ========================================================
// class GLFramebuffer: OGL Frame Buffer Object (FBO)
// ========================================================

class GLFramebuffer final
{
public:

    // Copy/assignment is disabled.
    GLFramebuffer(const GLFramebuffer &) = delete;
    GLFramebuffer & operator = (const GLFramebuffer &) = delete;

    // TODO
private:
};

// ========================================================
// class GLBatchLineRenderer: 3D line drawing helper
// ========================================================

class GLBatchLineRenderer final
{
public:

    // Copy/assignment is disabled.
    GLBatchLineRenderer(const GLBatchLineRenderer &) = delete;
    GLBatchLineRenderer & operator = (const GLBatchLineRenderer &) = delete;

    // Initial batch size is not a fixed constraint. It will grow as needed.
    GLBatchLineRenderer(GLFWApp & owner, int initialBatchSizeInLines);

    // Add a one color line to the draw batch.
    void addLine(const Point3 & from, const Point3 & to, const Vec4 & color);

    // Add a two-tone lines segment to the draw batch.
    void addLine(const Point3 & from, const Point3 & to,
                 const Vec4 & fromColor, const Vec4 & toColor);

    // Actually performs the drawing.
    // Will make the lines shader program current.
    void drawLines();

    // Discards all batched lines. Call this after 'drawLines()'
    // if you're sending new data to the batch at every frame.
    void clear();

    // Get/set the model-view-projection used to render the lines.
    const Mat4 & getLinesMvpMatrix() const noexcept { return linesMvpMatrix; }
    void setLinesMvpMatrix(const Mat4 & mvp) noexcept { linesMvpMatrix = mvp; }

private:

    // Every 2 vertexes makes a line.
    std::vector<GLLineVertex> lineVerts;

    // Basic color-only shader.
    GLShaderProg linesShader;

    // The OpenGL draw buffers.
    GLVertexArray linesVA;

    // Usually just a view+projection, but can have other transforms.
    Mat4  linesMvpMatrix;
    GLint linesMvpMatrixLocation;

    // We only send new data to GL if the batch was modified.
    bool needGLUpdate;
};

// ========================================================
// class GLBatchPointRenderer: 3D point drawing helper
// ========================================================

class GLBatchPointRenderer final
{
public:

    // Copy/assignment is disabled.
    GLBatchPointRenderer(const GLBatchPointRenderer &) = delete;
    GLBatchPointRenderer & operator = (const GLBatchPointRenderer &) = delete;

    // Initial batch size is not a fixed constraint. It will grow as needed.
    GLBatchPointRenderer(GLFWApp & owner, int initialBatchSizeInPoints);

    // Add a colored point to the draw batch.
    void addPoint(const Point3 & point, float size, const Vec4 & color);

    // Actually performs the drawing.
    // Will make the points shader current.
    void drawPoints();

    // Discards all batched points. Call this after 'drawPoints()'
    // if you're sending new data to the batch at every frame.
    void clear();

    // Get/set the model-view-projection used to render the points.
    const Mat4 & getPointsMvpMatrix() const noexcept { return pointsMvpMatrix; }
    void setPointsMvpMatrix(const Mat4 & mvp) noexcept { pointsMvpMatrix = mvp; }

private:

    std::vector<GLPointVertex> pointVerts;

    // Basic color-only shader. Uses GL_PROGRAM_POINT_SIZE.
    GLShaderProg pointsShader;

    // The OpenGL draw buffers.
    GLVertexArray pointsVA;

    // Usually just a view+projection, but can have other transforms.
    Mat4  pointsMvpMatrix;
    GLint pointsMvpMatrixLocation;

    // We only send new data to GL if the batch was modified.
    bool needGLUpdate;
};

// ========================================================
// class GLBatchTextRenderer: Simple 2D screen text draw
// ========================================================

class GLBatchTextRenderer final
{
public:

    // Copy/assignment is disabled.
    GLBatchTextRenderer(const GLBatchTextRenderer &) = delete;
    GLBatchTextRenderer & operator = (const GLBatchTextRenderer &) = delete;

    // Initial batch size is not a fixed constraint. It will grow as needed.
    GLBatchTextRenderer(GLFWApp & owner, int initialBatchSize);

    // Add a string to the text batch for later drawing.
    void addText(float x, float y, float scaling, const Vec4 & color, const char * text);
    void addTextF(float x, float y, float scaling, const Vec4 & color, const char * format, ...) ATTR_PRINTF_FUNC(6, 7);

    // Draw or clear the batched strings.
    void drawText(int scrWidth, int scrHeight);
    void clear();

    // Unscaled dimensions in pixels.
    float getCharHeight() const noexcept;
    float getcharWidth()  const noexcept;

private:

    struct TextString final
    {
        float       posX;
        float       posY;
        float       scaling;
        Vec4        color;
        std::string text;

        TextString(float x, float y, float s, const Vec4 & c, const char * str)
            : posX    { x }
            , posY    { y }
            , scaling { s }
            , color   { c }
            , text    { str }
        { }
    };

    void pushStringGlyphs(float x, float y, float scaling, const Vec4 & color, const char * text);
    void pushGlyphVerts(const GLDrawVertex verts[4]);

    std::vector<TextString>   textStrings;
    std::vector<GLDrawVertex> glyphsVerts;
    GLTexture                 glyphsTexture;
    GLVertexArray             glyphsVA;
    GLShaderProg              glyphsShader;
    GLint                     glyphsShaderScreenDimensions;
    GLint                     glyphsShaderTextureLocation;
    bool                      needGLUpdate;
};

// ========================================================
// class GLError: Exception type thrown by GLFWApp
// ========================================================

class GLError final
    : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
    ~GLError();
};

// ========================================================
// class GLFWApp: Base class for the GLFW application
// ========================================================

class GLFWApp
{
public:

    enum class MouseButton
    {
        Left,
        Right,
        Middle
    };

    using Ptr = std::unique_ptr<GLFWApp>;

    // Copy/assignment is disabled.
    GLFWApp(const GLFWApp &) = delete;
    GLFWApp & operator = (const GLFWApp &) = delete;

    GLFWApp(int winWidth, int winHeight,
            const float * clearColor = nullptr,
            std::string title = "");

    virtual ~GLFWApp();

    //
    // Methods overridable by the application:
    //

    // Startup/shutdown:
    // onInit() is called right after successful window creation.
    // onShutdown() is called BEFORE main() exits. Gets called even if there's an exception.
    virtual void onInit();
    virtual void onShutdown();

    // Called before onFrameRender.
    virtual void onFrameUpdate(std::int64_t currentTimeMillis,
                               std::int64_t elapsedTimeMillis);

    // Called after onFrameUpdate.
    virtual void onFrameRender(std::int64_t currentTimeMillis,
                               std::int64_t elapsedTimeMillis);

    // Input events (fired before onFrameUpdate):
    virtual void onMouseMotion(int x, int y);
    virtual void onMouseScroll(double xOffset, double yOffset);
    virtual void onMouseButton(MouseButton button, bool pressed);
    virtual void onKey(int key, int action, int mods);
    virtual void onKeyChar(unsigned int chr);

    //
    // Miscellaneous application and OGL helpers:
    //

    // Get the time (in milliseconds) elapsed since the application started.
    std::int64_t getTimeMilliseconds() const noexcept;

    // Logs errors if glGetError reports any.
    // Can optionally throw an exception if 'throwExcept' is true.
    void checkGLErrors(const char * function, const char * filename,
                       int lineNum, bool throwExcept);

    // Prints the message and returns normally.
    void printF(const char * format, ...) ATTR_PRINTF_FUNC(2, 3);

    // Prints the error and throws a GLError exception.
    void errorF(const char * format, ...) ATTR_PRINTF_FUNC(2, 3);

    // Takes ownership of the system cursor/pointer, hiding it.
    // Use restoreSystemCursor() to make it visible again.
    void grabSystemCursor();
    void restoreSystemCursor();

    //
    // Misc accessors:
    //

    int getWindowWidth()  const noexcept { return windowWidth;  }
    int getWindowHeight() const noexcept { return windowHeight; }

    GLFWwindow * getWindowPtr()          const noexcept { return glfwWindowPtr; }
    const std::string & getWindowTitle() const noexcept { return windowTitle;   }
    const float * getClearScrColor()     const noexcept { return clearScrColor; }

    void setWindowTitle(std::string title)      noexcept;
    void setClearScrColor(const float color[4]) noexcept;

    //
    // Internal use helpers:
    //

    void tryCreateWindow(); // Tries to create the GLFWwindow. Might throw GLError.
    void runMainLoop();     // Runs the event and render loop until the app window is closed.

private:

    // Common window and app data:
    int          windowWidth;
    int          windowHeight;
    float        clearScrColor[4];
    GLFWwindow * glfwWindowPtr;
    std::string  windowTitle;
};

// ========================================================
// AppFactory:
// This factory gets implemented by each application
// to create an extended GLFWApp instance with the
// specific application logic and data. This method
// is called soon after the program main() is entered.
// ========================================================

struct AppFactory final
{
    static GLFWApp::Ptr createGLFWAppInstance();
};

#endif // GL_UTILS_HPP

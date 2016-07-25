
-- Note: Only tested on MacOS.

------------------------------------------------------
-- Global projects flags / defines:
------------------------------------------------------

-- Project-wide compiler flags for all builds:
local BUILD_OPTS = {
    "-std=c++11",
    "-Wall",
    "-Wextra",
    "-Weffc++",
    "-Winit-self",
    "-Wformat=2",
    "-Wstrict-aliasing",
    "-Wuninitialized",
    "-Wunused",
    "-Wshadow",
    "-Wswitch",
    "-Wswitch-default",
    "-Wpointer-arith",
    "-Wwrite-strings",
    "-Wmissing-braces",
    "-Wparentheses",
    "-Wsequence-point",
    "-Wreturn-type",
    "-pedantic"
}

-- Project-wide Debug build switches:
local DEBUG_DEFS = {
    "DEBUG=1", "_DEBUG=1",  -- Enables assert()
    "_GLIBCXX_DEBUG",       -- GCC std lib debugging
    "_LIBCPP_DEBUG=0",      -- For Clang (libc++)
    "_LIBCPP_DEBUG2=0"      -- Clang; See: http://stackoverflow.com/a/21847033/1198654
}

-- Project-wide Release build switches:
local RELEASE_DEFS = {
    "NDEBUG=1"
}

-- System specific libraries:
local SYSTEM_LIBS = {
    -- Mac OSX frameworks:
    "CoreFoundation.framework",
    "OpenGL.framework",
    -- GLFW must be already installed!!!
    "GLFW3"
}

-- Target names:
local FRAMEWORK_LIB    = "Framework"
local DOOM3_MODELS_APP = "doom3_models"
local PROJ_TEX_APP     = "projected_texture"
local POLY_TRI_APP     = "poly_triangulation"
local WORLD_BSP_APP    = "world_bsp"

------------------------------------------------------
-- Common configurations for all projects:
------------------------------------------------------
workspace "GLCoreSamples"
    configurations { "Debug", "Release" }
    buildoptions   { BUILD_OPTS }
    language       "C++"
    location       "build"
    targetdir      "build"
    libdirs        "build"
    includedirs    { "source", "source/framework",  "source/vectormath", "source/gl3w/include" }
    filter "configurations:Debug"
        optimize "Off"
        flags    "Symbols"
        defines  { DEBUG_DEFS }
    filter "configurations:Release"
        optimize "On"
        defines  { RELEASE_DEFS }

------------------------------------------------------
-- Framework static library:
------------------------------------------------------
project (FRAMEWORK_LIB)
    kind  "StaticLib"
    files { "source/framework/**.hpp", "source/framework/**.cpp", "source/gl3w/src/**.cpp" }

------------------------------------------------------
-- DOOM3 model viewer:
------------------------------------------------------
project (DOOM3_MODELS_APP)
    kind  "ConsoleApp"
    files { "source/doom3_models.cpp" }
    links { SYSTEM_LIBS, FRAMEWORK_LIB }

------------------------------------------------------
-- Projected textures demo:
------------------------------------------------------
project (PROJ_TEX_APP)
    kind  "ConsoleApp"
    files { "source/projected_texture.cpp" }
    links { SYSTEM_LIBS, FRAMEWORK_LIB }

------------------------------------------------------
-- Polygon triangulation demo:
------------------------------------------------------
project (POLY_TRI_APP)
    kind  "ConsoleApp"
    files { "source/poly_triangulation.cpp" }
    links { SYSTEM_LIBS, FRAMEWORK_LIB }

------------------------------------------------------
-- BSP scene rendering demo:
------------------------------------------------------
project (WORLD_BSP_APP)
    kind  "ConsoleApp"
    files { "source/world_bsp.cpp" }
    links { SYSTEM_LIBS, FRAMEWORK_LIB }


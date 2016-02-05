
// ================================================================================================
// -*- C++ -*-
// File: gl_main.cpp
// Author: Guilherme R. Lampert
// Created on: 14/11/15
// Brief: Application entry point and GLFW callbacks.
//
// This source code is released under the MIT license.
// See the accompanying LICENSE file for details.
//
// ================================================================================================

#include "gl_utils.hpp"

#include <iostream>
#include <cstdlib>

// Declared here to be easily accessible from the GLFW callbacks.
static GLFWApp::Ptr g_AppInstance{};

// ========================================================
// GLFW callback functions:
// ========================================================

extern "C"
{

void mousePosCallback(GLFWwindow * window, const double xpos, const double ypos)
{
    assert(window == g_AppInstance->getWindowPtr());
    g_AppInstance->onMouseMotion(static_cast<int>(xpos), static_cast<int>(ypos));
}

void mouseScrollCallback(GLFWwindow * window, const double xoffset, const double yoffset)
{
    assert(window == g_AppInstance->getWindowPtr());
    g_AppInstance->onMouseScroll(xoffset, yoffset);
}

void mouseButtonCallback(GLFWwindow * window, const int button, const int action, const int mods)
{
    assert(window == g_AppInstance->getWindowPtr());

    // Unused for now...
    (void)mods;

    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT :
        g_AppInstance->onMouseButton(GLFWApp::MouseButton::Left, (action == GLFW_PRESS));
        break;

    case GLFW_MOUSE_BUTTON_RIGHT :
        g_AppInstance->onMouseButton(GLFWApp::MouseButton::Right, (action == GLFW_PRESS));
        break;

    case GLFW_MOUSE_BUTTON_MIDDLE :
        g_AppInstance->onMouseButton(GLFWApp::MouseButton::Middle, (action == GLFW_PRESS));
        break;

    default :
        g_AppInstance->errorF("Invalid GLFW button enum in mouse callback! %X", button);
        break;
    } // switch (button)
}

} // extern C

// ========================================================
// Program main():
// ========================================================

int main()
{
    g_AppInstance = AppFactory::createGLFWAppInstance();
    if (g_AppInstance == nullptr)
    {
        std::cerr << "Null application instance!\n";
        return EXIT_FAILURE;
    }

    try
    {
        g_AppInstance->onInit();
        g_AppInstance->runMainLoop();
        g_AppInstance->onShutdown();
    }
    catch (std::exception & e)
    {
        std::cerr << "Unhandled exception: " << e.what() << "\n"
                  << "Terminating the application.\n";

        // Attempt cleanup. If this throws the program
        // will just terminate with and unhandled exception.
        g_AppInstance->onShutdown();
        return EXIT_FAILURE;
    }
}

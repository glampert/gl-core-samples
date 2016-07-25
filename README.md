
# Core OpenGL samples and tests

[![Build Status](https://travis-ci.org/glampert/gl-core-samples.svg)](https://travis-ci.org/glampert/gl-core-samples)

Core OpenGL samples and tests written in C++11.

![gl-core-samples](https://raw.githubusercontent.com/glampert/gl-core-samples/master/assets/samples.png "gl-core-samples")

## Contents

In the `source/` directory you will find:

- The `framework/` subdir, which contains code shared by all the sample applications.
- The `shaders/` subdir, which contains the GLSL shaders used by the sample applications.
- `doom3_models.cpp` is a simple viewer for MD5 models from the DOOM3 game, with support for skeleton animation.
- `poly_triangulation.cpp` is a sample testing a couple different polygon triangulation algorithms.
- `projected_texture.cpp` simulates a spotlight using projected texturing and a "light cookie" texture.
- `world_bsp.cpp` uses Binary Space Partitioning (BSP) and Portals to cull and render world geometry.
- Other third-party dependencies.

## License

This project's source code is released under the [MIT License](http://opensource.org/licenses/MIT).


# VUI Backends

Since VUI does not have any direct dependencies, it requires that the user has 3 different sub systems in their appliaction.

These are:
- a window & input sub system (eg. SDL2)
- a graphics sub system (eg. OpenGL)
- and a font layout/manager sub system (eg. stb_truetype)

To make it easier to integrate VUI in your project, we have a few library shims to bridge VUI with these sub systems. See the header file of the sets of files to see more documentation.


GENGINE
======

NOTICE: Not in active development anymore lol.

GENGINE (Not final name hopefully) is a compact, cross-platform game engine and editor that aims for a 2010s AAA look and feel. It is currently very primitive, but in very active development.

The project bundles an editor UI, a small runtime, and a simple scene format so you can prototype scenes, shaders and game logic fast.

Note: Most of the things said here are subject to change as the engine grows and I may not update this readme very often!

Highlights
---------

- Editor with a dockable interface.
- Runtime rendering using OpenGL (GLAD).
- Runtime shader compilation and simple hot-reload support for iterative shader development.
- Simple scene model with objects, textures and two light types (directional & point).
- Small set of utilities:  mesh generation, a minimal shader helper, and basic serialization as well as my nsmlib for filesystems and math.

Technologies
-------------------

- Language: C++
- Rendering: OpenGL (via GLAD)
- UI: ImGui
- Windowing/Input: SDL2
- Serialization: nlohmann::json
- Image loading: stb_image
- Build system: CMake

Project layout (brief)
----------------------

- `Engine/` - core engine and editor source files
- `include/` - public headers, bundled third-party headers
- `shaders/` - project shaders (lit/unlit)
- `textures/` - example textures used by the sample scene
- `source/` - game code

Why this project?
------------------

GENGINE aims to be an approachable, low‑ceremony environment for learning and experimenting with rendering, shader development and game tools. It keeps the core small and readable so students, hobbyists and engine tinkers can explore and modify the internals easily. The engine is a part of every project, so everything can be customized by the user however they want.

Want to dive deeper?
---------------------

Read the wiki for build steps, editor controls, shader workflow and troubleshooting pages.

License
-------

See the `LICENSE` file in the project root for license terms. Other Licenses can be found in the OTHER_LICENSES directory.

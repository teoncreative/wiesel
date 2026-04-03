# Wiesel Engine

Wiesel is a cross-platform game engine built with C++ and Vulkan, featuring a deferred rendering pipeline, C# scripting, and a full-featured editor.

## Features

- **Deferred PBR rendering** with render graph, cascaded shadow maps, ray-traced shadows, SSAO, bloom, motion blur,
  TAA/FXAA, IBL, and forward transparency with IBL support
- **Jolt Physics** with multi-threaded simulation, box/sphere/capsule/mesh/heightfield colliders, baked mesh collider
  assets, triggers, one-way platforms, raycasting, and overlap queries
- **C# scripting** via Mono with hot-reload, lifecycle callbacks, collision/trigger events, and exception handling
- **Editor** with scene hierarchy, component inspector, asset browser, prefabs, play/stop with snapshot restore, gizmos,
  GPU memory stats, and developer console
- **Audio** via miniaudio with 3D spatial sound, reverb zones, and volume buses
- **Skeletal animation** with state machine, crossfading, and transition conditions
- **Sprite rendering** with atlas support, frame animation, and sprite sheet slicing
- **UI system** with RmlUi integration, canvas layout, anchors/pivots, text rendering, buttons, text input, gamepad
  navigation, and data binding from C#
- **Input system** with data-driven contexts, multi-player support, gamepad mapping
- **Asset system** with VFS, stable handles via .meta files, unified async loading pipeline, thread-safe asset manager,
  texture quality with VRAM savings, and .pak archive support
- **Game export** with asset packing, script compilation, and standalone runtime
- **Left-handed coordinate system** (+Z forward)

## Building

### Requirements
- C++23 compiler (MSVC, GCC, or Clang)
- CMake 3.24+
- Vulkan SDK
- Mono runtime

### Build
```bash
git clone --recursive https://github.com/teoncreative/wiesel.git
cd wiesel
cmake -B build
cmake --build build
```

## Sample Projects

Sample projects are maintained in a separate repository:

https://github.com/teoncreative/wiesel-samples

## Project Structure

```
wiesel/          Engine library (headers, source, shaders, C# scripts)
editor/          Editor application
runtime/         Standalone game runtime
vendor/          Third-party dependencies (git submodules)
libs/            Internal libraries (wpak, monolib)
tools/           Asset packer and build tools
```

## Third-Party Libraries

[Vulkan](https://www.vulkan.org/),
[Jolt Physics](https://github.com/jrouwe/JoltPhysics),
[SDL3](https://www.libsdl.org/) / [GLFW](https://www.glfw.org/),
[Dear ImGui](https://github.com/ocornut/imgui),
[entt](https://github.com/skypjack/entt),
[Assimp](https://github.com/assimp/assimp),
[FreeType](https://freetype.org/),
[miniaudio](https://miniaud.io/),
[glslang](https://github.com/KhronosGroup/glslang),
[VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator),
[nlohmann/json](https://github.com/nlohmann/json),
[Tracy](https://github.com/wolfpld/tracy),
[Mono](https://www.mono-project.com/),
[stb](https://github.com/nothings/stb),
[Recast Navigation](https://github.com/recastnavigation/recastnavigation)

## License

Apache License 2.0

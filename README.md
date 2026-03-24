# Wiesel Engine

Wiesel is a cross-platform game engine built with C++ and Vulkan, featuring a deferred rendering pipeline, C# scripting, and a full-featured editor.

## Features

### Rendering
- **Deferred rendering pipeline** with render graph architecture
- **Physically-based rendering** with metallic/roughness workflow
- **Shadow mapping** with cascaded shadow maps and ray-traced shadows
- **Post-processing**: SSAO, bloom, motion blur, TAA, FXAA
- **Forward transparency pass** for semi-transparent meshes
- **Sprite rendering** with atlas and frame animation support
- **Skybox system** with panorama, cubemap, and cross layout support
- **Orthographic and perspective** camera projection modes
- **Debug overlays** with translucent textured collider/trigger/reverb zone visualization

### Audio
- **miniaudio backend** with custom VFS integration
- **3D spatial audio** with listener positioning and distance attenuation
- **AudioSourceComponent** for entity-attached sounds
- **Reverb zones** with delay-based DSP and distance blending
- **Volume buses** (Master, SFX, Music) with per-bus control
- **C# Audio API** for fire-and-forget playback, music, and AudioClip drag-drop fields

### Physics
- **Bullet Physics** integration (3D rigid bodies, collision detection)
- **Box, sphere, and heightfield colliders** with trigger support
- **Raycasting and overlap queries** accessible from C#
- **Contact callbacks** (OnCollisionEnter/Stay/Exit, OnTriggerEnter/Stay/Exit)

### Scripting
- **C# scripting** via Mono with hot-reload
- **NativeBehavior** C++ scripting for performance-critical logic
- **Lifecycle callbacks**: OnStart, OnUpdate, OnDisable, OnDestroy
- **Exception handling** with auto-disable on error (prevents error spam)
- **Script field inspector** with drag-drop support for assets (AudioClip, Prefab, Entity)

### Input
- **Data-driven input system** with named contexts (keyboard, gamepad profiles)
- **Multi-player support** (up to 4 players with independent context assignment)
- **Gamepad support** with button/axis mapping and analog sticks
- **Project-level input configuration** with string-based key serialization

### Editor
- **Scene hierarchy** with drag-drop entity reordering and parenting
- **Component inspector** with per-type editors and Add Component dropdown
- **Asset browser** with thumbnails, import, drag-drop, and right-click context menu
- **Project Settings** popup (Scene, Rendering, Input categories)
- **Prefab system** with editor, instantiation, and serialization
- **Play/Stop** with full scene snapshot restore
- **Editor camera** with free-look, grid overlay, and gizmos (translate/rotate/scale)
- **Developer console** with C#-registered commands
- **Idle FPS throttling** to reduce power usage when inactive
- **Discord Rich Presence** integration (optional)

### UI / Canvas
- **Screen-space UI** with canvas layout system (row/column, alignment, spacing)
- **Components**: colored rectangles, textured images, text rendering
- **Anchor/pivot system** with size modes (fixed, percent, stretch)
- **Per-glyph text rendering** with FreeType font loading

### Animation
- **Skeletal animation** with up to 128 bones
- **Animation state machine** with transition conditions and crossfading
- **Parameter types**: Bool, Int, Float, Trigger

### Asset System
- **Virtual File System (VFS)** with mount points and archive support
- **Stable asset handles** via .meta files (binary assets) and embedded UUIDs (JSON assets)
- **Asset types**: Texture, Model, Material, Sprite, Skybox, Font, Script, Audio, Scene, Prefab
- **Reusable AssetCombo widget** with thumbnail hover previews

## Building

### Requirements
- C++23 compiler (MSVC, GCC, or Clang)
- CMake 3.20+
- Vulkan SDK
- Mono (for C# scripting)

### Build
```bash
git clone --recursive https://github.com/teoncreative/wiesel.git
cd wiesel
cmake -B build
cmake --build build
```

### Window Backends
- **SDL3** (default): `-DWIESEL_WINDOW_BACKEND=SDL3`
- **GLFW** (legacy): `-DWIESEL_WINDOW_BACKEND=GLFW`

## Project Structure

```
wiesel/                  # Engine library
  include/               # Public headers
    audio/               # Audio system
    rendering/           # Renderer, camera, textures, pipeline
    scene/               # ECS, components, serialization
    physics/             # Physics world, colliders, rigidbody
    script/              # C# scripting (Mono)
    input/               # Input manager
    ui/                  # Canvas/UI system
    animation/           # Skeletal animation
    asset/               # Asset manager, handles
    behavior/            # NativeBehavior C++ scripting
    util/                # VFS, logging, utilities
  src/                   # Implementation
  assets/
    scripts/             # Engine C# scripts (Input, Audio, Entity, etc.)
    shaders/             # GLSL shaders (compiled at runtime via glslang)
    textures/            # Default engine textures
    fonts/               # Default fonts

editor/                  # Editor application
examples/                # Example projects
  native_demo/           # Demo with NativeBehavior + project file
vendor/                  # Third-party dependencies
```

## Third-Party Libraries

- [Vulkan](https://www.vulkan.org/) - Graphics API
- [SDL3](https://www.libsdl.org/) / [GLFW](https://www.glfw.org/) - Window management
- [Dear ImGui](https://github.com/ocornut/imgui) - Editor UI
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) - 3D gizmos
- [entt](https://github.com/skypjack/entt) - Entity Component System
- [Bullet Physics](https://github.com/bulletphysics/bullet3) - Physics engine
- [Assimp](https://github.com/assimp/assimp) - Model loading
- [FreeType](https://freetype.org/) - Font rendering
- [miniaudio](https://miniaud.io/) - Audio playback
- [nlohmann/json](https://github.com/nlohmann/json) - JSON parsing
- [glslang](https://github.com/KhronosGroup/glslang) - Runtime shader compilation
- [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) - GPU memory management
- [stb](https://github.com/nothings/stb) - Image loading
- [Tracy](https://github.com/wolfpld/tracy) - Profiling
- [Mono](https://www.mono-project.com/) - C# runtime
- [discord-rpc](https://github.com/discord/discord-rpc) - Discord Rich Presence

## License

Apache License 2.0
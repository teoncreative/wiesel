# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

The project uses CMake with Ninja and C++23. Build presets are defined in `CMakePresets.json`.

```bash
# Configure
cmake --preset debug    # Debug build (Tracy, Vulkan validation, GPU profiling)
cmake --preset release  # Release build

# Build
cmake --build --preset debug
cmake --build --preset release

# Build specific targets
cmake --build --preset debug --target wiesel-editor
cmake --build --preset debug --target wiesel-runtime
cmake --build --preset debug --target assetpacker

# Create editor bundle (packs assets, compiles scripts, copies runtime + mono)
cmake --build --preset debug --target bundle-editor
```

There is no test suite. Testing is manual via the editor and runtime.

## Coding Style

- **C++**: Google naming - `snake_case` for variables/functions, `CamelCase` for types, `kConstantName` for constants, `member_name_` for class members
- **C#**: Standard C# conventions - `PascalCase` for methods/properties, `camelCase` for locals
- **GLSL**: `camelCase` for variables
- Never put multiple statements on one line - each statement gets its own line
- Always use braces for if/else/for/while bodies, even single statements
- Never use em dashes (-) in comments or code - use regular hyphens
- Only use `auto` when the type is obvious from the right-hand side (e.g. `auto& tc = entity.GetComponent<TransformComponent>()`)
- Don't use numbered step comments (e.g. `// 1. Do X`, `// 2. Do Y`)
- Logging macros: `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR` (no LOG_TRACE)

## Architecture

Wiesel is a C++ game engine with Vulkan rendering, Jolt Physics, and C# scripting via Mono. Left-handed coordinate system (+Z forward). All engine code is in the `Wiesel` namespace.

### Project Layout

- `wiesel/` - Static engine library (core systems, shaders, C# runtime scripts)
- `editor/` - Editor executable (ImGui-based, split into ~8 source files by concern)
- `runtime/` - Standalone game runtime executable
- `libs/monolib/` - Mono C# runtime wrapper library
- `libs/wpak/` - Binary archive format library
- `tools/codegen/` - Reflection code generator (scans WCLASS/WPROPERTY macros)
- `tools/asset_packer/` - Asset packing tool
- `vendor/` - Third-party dependencies (git submodules)

### Engine Singleton

`Engine` is a static singleton (`w_engine.h`) that owns all subsystems: Renderer, VFS, AssetManager, ScriptManager, SceneManager, InputManager, AudioManager, UIManager, ThreadPool, etc. Access via `Engine::renderer()`, `Engine::asset_manager()`, etc.

### Rendering Pipeline

Deferred rendering with a render graph (`w_rendergraph.h`). Render features (`w_render_feature.h`) are pluggable components that register passes on the graph:

Geometry -> SSAO -> Lighting -> IBL -> Transparency -> Bloom -> TAA/FXAA -> Motion Blur -> Composite -> Canvas/Sprites -> Debug

Each feature implements `SetupResources()` and `AddPasses()`. Features register outputs by well-known string names (e.g. "GBuffer", "Lighting", "PipelineOutput") that downstream features look up via `RenderResourceRegistry`.

Per-camera resources are managed by `CameraResourcePool` (`w_camera.h`). Resource staleness is tracked via `pipeline_version_` on SceneManager - cameras detect version mismatches and rebuild their render graphs.

### ECS

Built on EnTT. Scenes own an `entt::registry`. Components are defined in `w_components.h`. Systems implement `ISystem` interface (`w_system.h`) and are priority-ordered. 13 built-in systems are registered in `Scene::RegisterSystems()`.

### Reflection

`WCLASS()` and `WPROPERTY(Serializable, Animatable, ReadOnly)` macros in headers are no-ops at compile time. `tools/codegen` scans them and generates `entt::meta` registration code.

### Asset System

- VFS (`w_vfs.h`) with mount points: `app://`, `engine://`, `editor://`. Supports physical dirs and `.pak` archives.
- `AssetManager` handles async loading via ThreadPool, dependency tracking, and version-based reloads.
- JSON asset types must use `AssetSerializerRegistry` (register in `w_asset_serializer.cc`), not manual parsing.
- `.meta` sidecar files provide stable UUID handles for assets.

### Scene Management

`SceneManager` supports additive scene loading (`LoadSceneMode::Single`/`Additive`), async loading, and loading screens. `MultiScene` wrapper enables cross-scene entity iteration and multi-camera rendering. Entity IDs pack an 8-bit scene index + 24-bit entity ID for the pick buffer.

### C# Scripting

Mono runtime managed by `ScriptManager`. C# classes extend `MonoBehavior` with lifecycle callbacks (OnStart, OnUpdate, OnDisable, OnDestroy, collision/trigger/input callbacks). Scripts hot-reload in the editor. Compilation uses `dotnet build` (Roslyn), runtime is Mono.

### Shader Conventions

- Geometry vertex shader outputs world-space normals
- G-buffer normals encoded as `normal * 0.5 + 0.5` (UNORM), decoded as `rgb * 2.0 - 1.0`
- Depth buffer stores positive linear depth
- Color/albedo textures use `VK_FORMAT_R8G8B8A8_SRGB`; normal maps and data textures must use `VK_FORMAT_R8G8B8A8_UNORM`
- SSAO transforms world-space normals to view space with `mat3(cam.viewMatrix) * worldNormal`
- std140: never use `float arr[N]` (element stride rounds to vec4). `GLM_FORCE_DEFAULT_ALIGNED_GENTYPES` is enabled so `glm::mat3` = 48 bytes, matching std140 layout

### Multi-Frame-in-Flight

2 frames in flight. `DeletionQueue` (`w_deletion_queue.hpp`) defers Vulkan resource destruction by N frames. Cross-frame `render_order_semaphores_` serialize GPU execution for shared render targets.

### Editor

Editor is `EditorLayer` pushed onto the Application layer stack. Split into 8 files: core, viewport, hierarchy, inspector, panels, menus, project, code. Uses ImGui with FreeType font rendering. Undo/redo via `CommandStack` with `IEditorCommand` pattern.
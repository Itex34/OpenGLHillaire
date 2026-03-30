# [Unreal Engine](https://www.unrealengine.com) Sky Atmosphere Rendering Technique

This is the project accompanying the paper `A Scalable and Production Ready Sky and Atmosphere Rendering Technique` presented at [EGSR 2020](https://egsr2020.london/program/).
It can be used to compare the new technique proposed in that paper to [Bruneton et al.](https://github.com/ebruneton/precomputed_atmospheric_scattering) as well as to a real-time path tracer.
The path tracer is using a simple and less efficient noise because I was not sure I could share noise code I had before.
The technique is used to render sky and atmosphere in [Unreal Engine](https://www.unrealengine.com).

## Build with CMake
1. Update git submodules: `git submodule update --init`
2. Configure and build with presets:
- `cmake --preset windows-msvc-release`
- `cmake --build --preset build-windows-release`
3. Run:
- `build/msvc/Release/PbrSkyViewer.exe`

Alternative presets:
- `windows-msvc-debug` + `build-windows-debug`
- `ninja-debug` + `build-ninja-debug`
- `ninja-release` + `build-ninja-release`

Notes:
- Presets enable `SKYATMOS_FETCH_DEPS=ON` by default to fetch GLFW/GLM automatically.
- Runtime resources are copied next to the sample executable during build.

## Runtime keys
- `Esc`: quit
- `F1`: toggle pointer lock (when FPS camera is enabled)
- `W/A/S/D + Space/Left Ctrl`: FPS camera movement
- `Shift`: movement speed boost

## Repository layout
- `include/PbrSkyLibOpenGL/SkyAtmosphereRenderer.h`: public library API.
- `src/SkyAtmosphereRenderer.cpp`: public wrapper implementation.
- `src/renderer/SkyRendererGl.{h,cpp}`: internal OpenGL sky-atmosphere renderer core.
- `samples/viewer/*`: GLFW + ImGui sample app and terrain renderer (`TerrainSceneRenderer`).
- `Resources/glsl/*`: shaders used by the library and sample.

The terrain path is now sample-owned. The library composites atmosphere using scene/shadow textures provided through `setExternalSceneTextures(...)` and `setExternalShadowMapTexture(...)`.

## Submodules
- [imgui](https://github.com/ocornut/imgui) V1.62 supported
- [tinyexr](https://github.com/syoyo/tinyexr)

## About the code
- The code of Eric Bruneton has been copied with authorization from his [2017 implementation](https://ebruneton.github.io/precomputed_atmospheric_scattering/).
- The code in this repository is provided in hope that later work using it to advance the state of the art will also be shared for every one to use.
- This code is currently configured primarily through `CMakeLists.txt`.

Thanks to [Epic Games](https://www.epicgames.com) for allowing the release of this source code.

The code is provided as is and no support will be provided.
I am still interested in any feedback, and we can discuss if you [contact me via twitter](https://twitter.com/SebHillaire).

[Seb](https://sebh.github.io/)

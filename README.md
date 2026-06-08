# DelaunayForge

An interactive C++ sandbox for building 2D planar domains, triangulating them, assigning boundary conditions, and solving PDE/FEM problems inside the editor.

The project is currently positioned as an experimental research tool rather than a polished product. The codebase already includes:

- interactive outer and inner boundary editing
- parametric, smooth-stroke, and fractal domain generation
- porous domains and random inner-boundary generation
- PDE solving and mesh-coupled solution inspection
- FEM error-analysis tooling, including Richardson-Aitken workflows
- LLGL as rendering backend

## Status

- Primary tested environment: macOS
- Graphics API: Metal on MacOS; D3D12 on Windows
- Project/archive formats are still evolving, so backward compatibility for saved assets is not guaranteed yet

## Repository Layout

- `src/`: application, editor, geometry, FEM, renderer, object system, and file I/O
- `include/`: vendored headers and small embedded third-party libraries
- `shaders/`: shader sources
- `fem_project/`: example or working project data
- `cmake/`: CMake helper modules used by the top-level build

## Getting Started

Clone the repository and initialize submodules first:

```bash
git clone <your-repo-url>
cd <repo-folder>
git submodule update --init --recursive
```

Configure and build with presets using default CMake generator:

```bash
cmake --preset default
cmake --build --preset release
```

Configure and build using Visual Studio 2022/2026:
```bash
cmake --preset vs2022
cmake --build --preset vs2022-release
```

The binary will be generated under `/bin/{BuildConfigName}/` and will be named `DelaunayForge`.

All configure presets:

- `default`
- `vs2022`
- `vs2026`

All build presets for default configuration:

- `debug`
- `release`
- `releaseWithDebugInfo`
- `minSizeRel`

All build presets for vs2022/vs2026 configurations:
- `vs2022-debug`
- `vs2022-release`
- `vs2022-releaseWithDebugInfo`
- `vs2022-minSizeRel`
- `vs2026-debug`
- `vs2026-release`
- `vs2026-releaseWithDebugInfo`
- `vs2026-minSizeRel`

## Dependencies

This repository uses a mix of submodules, vendored code, and CMake `FetchContent`:

- ImGui is included as a git submodule in `include/imgui`
- `glfw`, `glm`, and `LLGL` are fetched by CMake
- `glad` and `lz4` are vendored in `include/`

Each third-party dependency remains under its own license terms.

## Acknowledgments

Shoutout to [JeFFlidan](https://github.com/JeFFlidan) for work on the graphics engine.

## Open Source Release Notes

This repository has a lot of active experimentation in it. For a first public release, the practical baseline is:

- keep generated build outputs out of git
- keep the top-level build short and readable
- document the supported build paths
- make it explicit that save-file compatibility is not stable yet
- pick a permissive starter license unless you want copyleft

## License

This repository is released under the MIT License. See `LICENSE`.
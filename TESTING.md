# Testing guide for mesh-oriented scenes

This repository ships several mesh-focused scenes (IDs 17–21) that exercise the OBJ loader, triangle shading, BVH acceleration, and instancing. Use the steps below to compile and run them so you can visually verify each feature.

## Build the renderer

1. Ensure SDL2 development files are available (required by CMake).
2. Configure and build in release mode:
   ```bash
   cmake -S . -B build
   cmake --build build
   ```
3. Run the executable from the build directory, passing the scene ID (default is 11 if omitted):
   ```bash
   ./build/CGAssignment4 <scene_id>
   ```

## Scene-by-scene checks

### 17 — Mesh demo baseline
- **Command**: `./build/CGAssignment4 17`
- **What you should see**: the sample OBJ twice—a matte red version on the left and a slightly glossy metal copy in the center and a translated duplicate to the right. This confirms OBJ loading, material assignment, and translate-based instancing are wired into the pipeline.

### 18 — OBJ loader transform gallery
- **Command**: `./build/CGAssignment4 18`
- **Expectation**: three copies of the mesh at different positions and scales on a gray floor. Verifies per-instance translation/scale baked during OBJ loading.

### 19 — Triangle normal showcase
- **Command**: `./build/CGAssignment4 19`
- **Expectation**: two mesh statues under a bright overhead light. The left uses flat shading (hard edges), the right uses smooth normal interpolation (soft highlights). Confirms vertex-normal interpolation in triangle hits.

### 20 — Mesh BVH stress test
- **Command**: `./build/CGAssignment4 20`
- **Expectation**: a grid of many mesh instances (uniform material) spread across a large floor. Rendering should remain interactive; this scene stresses triangle AABB construction and the mesh-level BVH.

### 21 — Transform + instancing study
- **Command**: `./build/CGAssignment4 21`
- **Expectation**: four grouped mesh instances showing a base object plus translated clones on a neutral ground plane. Demonstrates that translate wrappers correctly reuse mesh BVH data.

## Tips for comparison
- Use consistent camera defaults for each scene; they are set in `select_scene` alongside sampling counts so you can compare noise levels and framing across runs.
- To contrast performance, time the render startup for scene 20 with and without BVH (temporarily replacing its world with a plain hittable list) to see the acceleration benefit.

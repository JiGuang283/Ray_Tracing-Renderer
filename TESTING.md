# Quick visual checks

Use these short runs to confirm the renderer behaves as expected after building with CMake.

## Scene 11 – PBR test spheres
- **Command:** `./build/CGAssignment4 11`
- **What you should see:** A checkerboard ground plane with three spheres in a line. From right to left they appear glossy blue, marbled (textured), and metallic yellow. The sky is bright blue.
- **Meaning:** Confirms default camera setup, environment background, texture sampling, and PBR materials render correctly.

## Scene 17 – Mesh demo
- **Command:** `./build/CGAssignment4 17`
- **What you should see:** A flat gray ground with three pyramid meshes. The left pyramid is matte red; two metal pyramids sit to the right (one shares the origin position, the other is translated rightward). The horizon is light blue and the camera looks slightly downward.
- **Meaning:** Verifies OBJ loading, per-mesh BVH acceleration, translation/instancing, and material assignment for mesh primitives.

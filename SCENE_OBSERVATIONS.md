# Scene 18/19 render observations

## Scene 18 — OBJ loader transform gallery
- **Expected** (per `select_scene`): three instances of `assets/sample_mesh.obj` scaled and translated across a gray XZ plane, each with distinct materials to showcase OBJ-space transforms baked at load time.
- **Observed**: screenshot shows a single large sphere on a checker-like procedural ground with marble noise; no multiple mesh instances or material variety are visible. This does not match the gallery’s intent and suggests either a different scene ID was rendered or the scene is not being reached.

## Scene 19 — Triangle normal showcase
- **Expected**: two triangle-based statues under an overhead light; left triangle uses flat shading with hard edges, right triangle uses interpolated vertex normals producing softer highlights.
- **Observed**: screenshot again shows one noise-textured sphere on a patterned ground plane. There are no planar triangles or contrasting shading styles, so the output diverges from the intended normal-interpolation test.

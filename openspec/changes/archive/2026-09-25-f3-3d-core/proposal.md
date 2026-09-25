# Proposal

**Roadmap position:** Implements milestone **F3 (3D core)** from
`openspec/specs/feature-roadmap`. The predecessor gate (F2) has passed on all
four targets (native 41/41 on Windows/Linux/macOS, Emscripten 23/23, web
goldens green), so this proposal is in roadmap order.

## Why

F2 delivered the first rendering layer plus the display list and the
golden-image harness, but the player draws in 2D only. F3 delivers the 3D
core named by the roadmap — camera, mesh resources, `drawMesh`, matrix math,
depth test, vertex colors, procedural primitives — the foundation F4
(lighting), F5 (render targets), F7 (skinning) all build on. Before writing
any proposal text, the provisional F3 API was re-reviewed with the user and
**the mesh data model changed by decision**: meshes become multi-surface
containers in the Godot sense (a mesh holds N surfaces; each surface owns its
attribute arrays and — from F4 — its own material), replacing the
single-attribute-set draft. Settling the surface model now avoids reshaping
the API twice later: glTF's `mesh.primitives[]` maps 1:1 onto surfaces (F6
import becomes structural), and per-surface materials (F4) have a home. The
same review removed the provisional global `setMaterial`: materials bind to
surfaces, never to engine state.

## What Changes

- **3D camera (`setCamera3D(opts)`)** — the one 3D camera is set, never
  created: `{ pos, target, fov, near?, far? }`, vertical fov in degrees,
  defaults `near` 0.1 / `far` 100, up vector +Y. Value-snapshot per record
  (ADR 0019), like the 2D camera. The F2 2D projection frame is a separate
  projection state: `setCamera3D` affects only 3D draws, so committed F2
  behavior and goldens are untouched. The vision.md "1 camera" limit applies
  to the 3D camera.
- **Multi-surface mesh data (`createMeshData`)** — **BREAKING vs the
  provisional contract**: MeshData becomes a container of 1..16 surfaces
  (fixed limit: 16 per mesh). Batch construction `{ surfaces: [...] }` or a
  single-surface shorthand `{ positions, ... }`; each surface (Godot
  "surface" / glTF "primitive") is `{ positions, normals?, uvs?, colors?,
  indices? }` with documented validation. An optional parallel
  `materials: [...]` array is accepted from F4 (material objects do not exist
  before F4) and becomes each surface's initial binding. Read-only query
  property `surfaceCount` on MeshData.
- **Mesh upload (`createMesh(meshData)`)** — uploads **all** surfaces to a
  GPU Mesh (opaque native-backed class per ADR 0011/0013; `destroy()`,
  GC finalizer backstop). Read-only query property `surfaceCount`. Surface
  material bindings carry over at upload.
- **Mesh drawing (`drawMesh(opts)`)** — `{ mesh, transform?, color? }` draws
  the whole mesh: every surface in surface order, depth-tested (3D records
  write and test depth; 2D records unchanged), `transform` a flat
  column-major 16-number array (default identity), `color` tint (default
  white) multiplying vertex colors where present. No single-surface draw
  (matches Godot).
- **Procedural primitives** — `makeCube(opts?)`, `makePlane(opts?)`,
  `makeSphere(opts?)` return single-surface MeshData, with pinned defaults
  so golden scenes are reproducible.
- **Script math layer** — `efx.mat4` / `efx.vec3` / `efx.quat` as
  engine-bundled **pure JS** (`[JS]` layer, ADR 0010: plain JS data in/out,
  degrees, column-major flat arrays, zero host dependencies).
- **Engine math** — GLM integrated per ADR 0005 (small C++-compiled
  translation units exposing a plain C API; GLM types never enter C11 TUs).
  This is F3's one new third-party dependency; evaluation per the roadmap
  requirement:

  **Dependency evaluation (roadmap requirement):** candidates: (a) **GLM**
  (header-only C++, MIT license; pins as a vendored snapshot under
  `vendor/` per ADR 0006; compiles on all four targets incl. Emscripten —
  header-only, widely used with emscripten; reaches C11 only through the
  ADR 0005 wrapper wall; already the accepted decision in ADR 0005).
  (b) cglm (C-native, MIT — no wrapper needed, but reverses ADR 0005 for no
  capability gain). (c) hand-rolled C math (no dependency, but a large,
  error-prone test surface for matrix code). Selection: (a), per ADR 0005.

- **Depth test** — 3D records enable the depth buffer (cleared each frame);
  playback order is preserved and depth resolves 3D overlap; equal-depth
  ties keep record order (deterministic).
- **Docs** — `docs/js-api.md` F3 section rewritten to the settled contract in
  the same change (js-api requirement), including the provisional-section
  ripples the review decided: F4's global `setMaterial` entry is **removed**
  in favor of `setMeshSurfaceMaterial(mesh, surfaceIndex, mat)` (Godot
  `surface_set_material` analog; surfaces without a binding render with an
  engine default material); F6 `loadMesh`/`loadMeshData` note the
  primitives→surfaces mapping; F7 joints/weights become per-surface
  attributes; F8 `drawModel(mesh, mat?, opts?)` is re-defined as a pure-JS
  convenience (bind `mat` to every surface, then `drawMesh`). AGENTS.md
  status table updated.

**Decisions doc:** new ADR `docs/decisions/0024` — multi-surface mesh data
model: surfaces, per-surface material bindings, no global material state
(supersedes the single-surface implication in ADR 0014's import mapping;
glTF primitives → surfaces). The efx-verb naming
(`setMeshSurfaceMaterial(mesh, i, mat)`, resource-first-arg, per the
`poseMesh` precedent) is recorded there with the method-form alternative.

**Non-goals:** lighting and the Phong material system (F4 — this change
settles only the *binding mechanism* and the surface slot); maps/alpha masks
(F4b — surface `uvs` are validated and stored from F3 but affect rendering
only from F4b); render targets and post FX (F5); zip root, file loading,
glTF import, REPL (F6); skinning/animation (F7 — including the per-surface
`joints`/`weights` semantics); text/high-level layer and the demo pack
(F8); texture sampling on meshes (no `drawMesh` texture option — textured
meshes arrive with F4b maps); input/audio (no spec); script-visible shaders
(never — ADR 0015); changing `2d-layer` behavior (F2 entries and goldens
are frozen).

## Capabilities

### New Capabilities

- `3d-core`: 3D drawing behavior — the perspective camera (state, defaults,
  value-snapshot, separation from the 2D frame), multi-surface mesh data
  (construction, validation, 16-surface limit, query property), mesh upload
  and lifecycle, whole-mesh drawing (surface order, depth test, transform,
  vertex colors/tint), procedural primitives with pinned defaults, and the
  pure-JS math layer contract.

### Modified Capabilities

- `js-api`: the resource-classification requirement gains the MeshData/Mesh
  read-only `surfaceCount` query properties (the current scenario pins every
  non-Texture class as having none) and the fixed-limits enumeration gains
  "16 surfaces per mesh".

## Impact

- **Code:** `src/render/` gains the 3D canned pipeline (sokol-shdc generated,
  ADR 0021), mesh GPU buffers, and depth-buffer playback; new `src/math/`
  module (GLM wrapper, C++ TUs + plain C API, ADR 0005/0003 walls);
  `src/api/api.c` gains the F3 bindings; `src/web/bridge.c` exposes the same
  functions with identical semantics (ADR 0022); runtime/resource machinery
  gains the MeshData/Mesh native classes per ADR 0011/0012.
- **Dependencies:** GLM vendored as a pinned snapshot under `vendor/`
  (evaluation above satisfies the roadmap's proposal-time requirement;
  ADR 0006 policy).
- **APIs:** F3 section of `docs/js-api.md` moves from provisional to current
  with the settled signatures; the provisional F4 `setMaterial` is replaced
  by `setMeshSurfaceMaterial` in the same document (same-change update
  required by the js-api spec); provisional F6/F7/F8 text adjusted to the
  surface model.
- **Verification:** new committed golden scenes (F3 gate = golden images +
  math unit tests on all four targets, per the rendering-milestone gate);
  headless unit tests extend the display-list record/assert suite to mesh
  records; math unit tests check the JS helpers against expected matrices.
  Note the verification-server quirk: new golden scenes need the manual
  server-side capture step before remote verification (llvmpipe only).
- **CI:** no workflow changes — the existing four-target gate
  (tag/manual-trigger only, ADR 0023) runs the new tests; standard order
  applies (Linux first, then Windows, then macOS; remote pre-verification
  via `tools/verify_remote.py` before dispatching).

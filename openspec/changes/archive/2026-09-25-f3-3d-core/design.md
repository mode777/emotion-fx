# Design

Implements F3 (3D core). Motivation and scope: `proposal.md`; behavior
contract: `specs/3d-core/` and `specs/js-api/`. Decisions here are numbered
D1–D12 and referenced by `tasks.md`.

## Context

`src/` is a core static lib (`platform`, `runtime`, `api`, `player`,
`render`) plus `main.c` (ADR 0003). F2 shipped the display list
(record arena, stable sort, record budget — ADR 0019), the 2D canned
pipeline as sokol-shdc generated shaders (ADR 0021), capture/golden harness
(ADR 0020), and the resource machinery for native-backed classes
(ADR 0011/0012). GLM is decided (ADR 0005) but not yet vendored. The script
math layer is pure JS by ADR 0010; the web player drives the same core
through the `src/web/` bridge with the host JS engine (ADR 0022). The mesh
data model itself (multi-surface, per-surface material bindings, no global
material state) is a user decision recorded in the proposal and ADR 0024 —
this document covers its engineering.

## Goals / Non-Goals

- Goals: mesh data/upload/draw path for 1..16 surfaces; depth-tested 3D
  playback that leaves F2 records and goldens byte-identical; GLM behind the
  wrapper wall; JS math + primitives; four-target verification.
- Non-goals: lighting/material evaluation (F4 uses the bindings this change
  stores), maps/uv sampling (uvs validated + stored now, consumed in F4b),
  mesh LOD/instancing, index formats beyond `uint32` in the native layer
  (scripts always pass plain arrays), animated/file loading (F6/F7).

## Decisions

- **D1 — Surface storage: one interleaved vertex buffer + one index buffer
  per surface, drawn as one sokol draw per surface.** Alternative (single
  mesh-level buffer with surface ranges) saves allocations but complicates
  per-surface upload lifetimes and F4 per-surface bindings; at PS2-era sizes
  (≤16 surfaces) the per-surface buffers are simpler and cache-friendly
  enough. Vertex layout per surface is built at upload from the MeshData
  attributes actually present (position always; normal/uv/color slots
  present-or-defaulted) so F4 can enable map sampling without re-laying-out.
- **D2 — GLM wrapper: new `src/math/` module.** C++-compiled TUs exposing
  `efx_math.h` (plain C: `efx_mat4`, `efx_vec3` ops used by camera and
  playback; no GLM types in headers). GLM vendored pinned under `vendor/glm`
  (header-only; only the used submodules are tracked). Used by: camera
  view-projection composition, record-time camera snapshot matrices, MVP in
  playback. The JS math layer does NOT use it (ADR 0010 — scripts get pure
  JS; duplication of a handful of formulas is accepted and covered by
  cross-check unit tests so script and engine math agree).
- **D3 — Camera math: `lookAt(pos, target, +Y up)` × `perspectiveY(fovY,
  aspect, near, far)`.** `fovY` degrees → radians internally; `aspect` =
  current target extent (window or render target) width/height, evaluated at
  record time and re-evaluated at playback time only for the active pass —
  records store pos/target/fov/near/far, playback composes (keeps records
  small and resize-correct). Alternative (store composed matrices) breaks
  window resize between record and playback.
- **D4 — Depth strategy: one depth buffer per pass; 3D records test+write,
  2D records neither.** sokol depth state: 3D pipeline variant
  `compare: less-equal, write: true`; the 2D variants keep F2 state
  (`compare: always, write: false`). Depth cleared each pass to 1.0 alongside
  the F2 clear color. Equal-depth ties resolve by record order because the
  playback order is record order (F2 never reorders; ADR 0019/D3) with
  `less-equal`. 2D draws over 3D content work naturally (painter's order on
  top, no depth write) — HUD-style layering without a second camera.
- **D5 — Canned shader: extend the single-source GLSL with a 3D variant**
  (sokol-shdc, ADR 0021): attributes position/normal/uv/color; uniforms MVP
  (premultiplied at playback: VP × record transform) and tint; fragment
  output = tint × vertex-color (default white), unlit. Normals/uv are bound
  but unused in F3 — they enter the layout now so F4 adds lighting/maps as
  uniform/shader-permutation work only. Alternative (separate minimal F3
  shader) means a second layout migration in F4.
- **D6 — Winding and culling: CCW front faces, backface culling ON for 3D
  records.** Primitives and the documented convention emit CCW outward;
  culling is deterministic across backends (GL/D3D11/Metal/GLES all support
  it) and halves fragment work. Custom meshes wound CW render inside-out —
  documented in js-api.md; no runtime winding check (cost not justified).
- **D7 — Transform representation: flat 16-number column-major array,
  validated at record time, stored as `float[16]` in the record.** Matches
  `efx.mat4` output and GLM's column-major storage; playback multiplies
  VP × model on the CPU (one mat4 mul per record, cheap at PS2-era counts).
- **D8 — MeshData native class: engine-owned contiguous storage copied from
  the JS arrays at creation** (GC pressure accounting per ADR 0012 — CPU
  bytes count). Validation happens once at creation (spec table: counts →
  `RangeError`, element types → `TypeError`, unknown fields → `TypeError`).
  Typed arrays are accepted beside plain arrays (F2 precedent) via the
  existing quickjs/bridge array-marshaling helpers; the web bridge must
  accept both identically (shared validation code in `src/api`).
- **D9 — Primitives pinned for goldens:** cube = 24 verts/36 indices,
  per-face normals, per-face 0..1 uvs; plane = `(segments+1)²` grid in XZ,
  +Y facing, CCW seen from above; sphere = standard UV sphere, `segments`
  latitude rings × `segments` longitude slices, poles as degenerate-ring
  fans (indexed, so no zero-area quads), normals = normalized positions,
  uvs equirectangular (u = lon/360°, v = lat/180°). Exact formulas live in
  the JS implementation; golden scenes freeze the outputs.
- **D10 — JS math layer: one bundled ES6 file per `efx.mat4`/`efx.vec3`/
  `efx.quat`**, loaded with the engine-provided script prelude on both
  bindings (desktop runtime prelude + web bridge prelude — same source,
  same semantics, per the F2 layering). Cross-check unit tests (task 8)
  assert agreement with the C wrapper for a fixed vector of inputs.
- **D11 — Golden scenes for F3** (committed under `tests/goldens/`, capture
  at the standard 640×480, empty update hook — scenes are static so frame 2
  equals frame 1 across targets, per the F2 determinism pattern):
  (1) `cube3d` tinted rotated procedural cube; (2) `vertcolor3d` vertex
  colors × tint; (3) `surfaces3d` two-surface mesh showing per-surface
  geometry; (4) `depth3d` depth-overlap scene (nearer mesh recorded before
  the farther one); (5) `transform3d` transform variety
  (translate/rotate/scale); (6) `plane3d` plane+segments grid. Scene
  scripts use only the settled F3 API; new scenes need the manual
  server-side capture step (llvmpipe) before remote verification per
  `docs/verification-server.md`.
- **D12 — 2D/3D camera separation (spec: 3D camera requirement).** The 2D
  camera state and the 3D camera state are separate record-time snapshots;
  `drawQuad` never reads the 3D camera. This preserves committed F2 goldens
  and keeps HUD layering trivial (D4). Alternative (one mode-tagged camera
  slot) makes every 2D-over-3D frame thrash the camera and risks F2
  regressions.

## Risks / Trade-offs

- [Golden churn from depth/clear changes on shared pass setup] → the depth
  clear is additive; 2D-only scenes render identically (verify F2 golden
  suite unchanged in tasks).
- [Per-backend depth semantics drift (D3D11/Metal vs GL)] → sokol
  normalizes depth *state* (compare/write) but not the clip-space depth
  *range*: GL clips z to [-w, w], D3D11/Metal to [0, w]. Playback folds the
  remap into the MVP on the 0..1 backends (`origin_top_left` backends) as
  row2 = 0.5·row2 + 0.5·row3 across all four columns — row 3 carries the
  perspective w in every column, so a translation-only fold clips the near
  half of every mesh (the Windows/macOS hollow-cube symptom). The
  four-target golden gate (ADR 0020) is the arbiter; depth scenes are part
  of the committed set. Durable rule: ADR 0025.
- [GLM vendoring size] → vendor only used headers (`vec3`/`vec4`/`mat4`)
  with a pinned commit hash recorded in `vendor/README.md`.
- [JS math duplication drifting from engine math] → cross-check unit tests
  over a fixed input vector (D10), run on all four targets.
- [Record budget impact of per-surface draws] → a 16-surface mesh records
  one mesh record (not 16); playback expands per surface after the budget
  check — budgets stay predictable for scripts.

## Migration Plan

No data migration (no shipped scripts depend on F3 API). The provisional
`docs/js-api.md` sections change in the same change (F3 → current; F4
`setMaterial` → `setMeshSurfaceMaterial`; F6/F7/F8 wording). Rollback is a
revert of the change branch; goldens are additive files.

## Open Questions

None — the API-shape questions were settled with the user in the proposal
review (multi-surface batches, per-surface materials with no global state,
16-surface cap, efx-verb naming, `materials` array accepted from F4).

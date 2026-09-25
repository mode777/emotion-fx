# 0025 — The engine owns the clip-space depth-range remap; sokol normalizes depth state, not depth range

Status: Accepted (2026-09, change `f3-3d-core`)

Supports: vision.md — cross-platform compatibility; roadmap — F3 3D core,
F5 render targets (offscreen attachment formats).

## Context

F3's mesh path rendered correctly on every GL backend (Linux llvmpipe,
Emscripten WebGL2) and drew hollow sliver-triangles on Windows (D3D11)
and macOS (Metal) — roughly forty CI-driven diagnostic commits went
looking for the fault in attribute layouts, uniform packing and index
formats. The cause was one line of matrix algebra in
`src/platform/pipeline.c`: GL clips depth to $z \in [-w, w]$, while
D3D11 and Metal clip to $z \in [0, w]$. **sokol_gfx does not hide this
difference.** It normalizes depth *state* (`compare`, `write_enabled`,
`clear_value`) and the `origin_top_left` feature flag, but the
projection matrix a program feeds its shader must already produce the
right clip range for the active backend. Our fold onto $[0, w]$ rewrote
only the translation column (`mvp[10] *= 0.5; mvp[14] = 0.5*mvp[14] +
0.5*mvp[15]`), so the perspective $w$ that row 3 carries in *every*
column never entered $z'$. Any vertex nearer than the model origin got
$z' > w$ and was clipped by the far plane. sokol-samples never trip this
because HandmadeMath's `HMM_Perspective_RH_ZO` builds a 0..1 projection
outright. Full record: `openspec/changes/f3-3d-core/design.md` (Risks).

## Decision

1. Camera math in `src/math` stays GL-convention (`perspective` and
   `ortho` produce $z \in [-1, 1]$ NDC). This keeps the C wrapper, the
   pure-JS `efx.mat4` layer (ADR 0010) and their cross-check tests on a
   single convention.
2. The platform layer (`src/platform/pipeline.c`) folds the backend
   remap into the composed MVP at playback, **and only there**, when
   `sg_query_features().origin_top_left` is set (D3D11, Metal, WebGPU;
   never GL). The fold is a row operation applied to all four columns,
   in column-major storage:

   ```c
   for (int c = 0; c < 4; c++)
       mvp[c*4 + 2] = 0.5f * mvp[c*4 + 2] + 0.5f * mvp[c*4 + 3];
   ```

   i.e. $\text{row}_2 \leftarrow \tfrac12\,\text{row}_2 + \tfrac12\,\text{row}_3$,
   which maps $z_{clip} \in [-w, w]$ onto $[0, w]$ monotonically. Depth
   comparisons and record-order ties are unchanged.
3. Shaders never know about the remap (ADR 0021: one GLSL source, no
   per-backend flavors). No `#ifdef`/uniform switch for depth range may
   be added to `shaders/*.glsl`.
4. The same principle covers offscreen attachments: sokol validates but
   does not adapt pixel formats. Pipelines are built with the
   environment-default color/depth formats (the sapp swapchain's), so any
   engine-created attachment a pipeline draws into — the Metal golden
   capture texture today, F5 render targets tomorrow — MUST declare the
   matching `sg_pixel_format` (BGRA8 color on Metal/D3D11, RGBA8 on GL;
   `SG_PIXELFORMAT_DEPTH` for depth). Release builds skip sokol's
   validation layer, so a mismatch is silent, not an error.

## Consequences

- A symptom of *only the near half of every mesh missing* (or, with the
  bug mirrored, the far half) on D3D11/Metal while GL is correct is a
  depth-range fault, not a layout/uniform fault. Check the fold first.
- `depth3d` and the other 3D goldens run on Windows (WARP) and macOS in
  the gate and are the regression tripwire for this fold (ADR 0020).
- F5 render-target passes reuse the same playback path, so they inherit
  the fold without further work — but they must respect item 4.
- A future orthographic 3D camera or `ortho` on the 3D path needs no new
  code: the fold is a clip-space operation, independent of projection
  type.
- The remap is a CPU mat4 row operation per mesh record; at PS2-era
  record counts the cost is negligible.

## Rejected alternatives

- **Build 0..1 projections in `src/math` when the backend needs them**
  (HandmadeMath `_ZO` style). Rejected: `efx.mat4` in JS would then
  disagree with the engine on `perspective` output (ADR 0010 cross-check
  tests), and scripts snapshot camera *parameters*, not matrices (design
  D3), so the remap belongs where the matrix is composed — at playback.
- **Per-backend shader variant applying `z = z*0.5 + w*0.5` in the vertex
  shader.** Rejected: violates ADR 0021 (single GLSL source) and hides a
  platform concern in shader code.
- **Rely on sokol to normalize the range.** It does not, by design — the
  library owns pipeline/pass *state*, the application owns the matrices.
  This ADR exists because that boundary was misremembered once.

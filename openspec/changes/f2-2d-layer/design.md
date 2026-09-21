# Design

## Context

F1 delivered the player skeleton: windowed and `--script` run modes, the
`efx` namespace (`log`/`quit`/`args`), global `update`/`render` hook pickup,
and the four-target CI gate. No rendering exists yet; sokol is vendored but
only used for window/context creation. ADR 0019 (written during F2 design
exploration) already fixes the display list's recording semantics and
explicitly defers three open items to this change: the script-facing
sort-key UX, the record-arena budget/overflow policy, and the
pose-after-draw strictness question. The roadmap assigns F2 the deferred
golden-image tolerance and CI-determinism decisions. Module walls (ADR
0003), the resource model (ADR 0011/0012/0013), and the canned-shader
contract (ADR 0015) all apply.

## Goals / Non-Goals

**Goals:**

- A `src/render/` module: display list (record → sort → playback), the 2D
  canned pipeline, texture management, white texture — behind the ADR 0003
  walls, with the record side testable headlessly.
- Settled camera/quad math (transform order, pivots, y-down top-left
  convention) that F3+ can build on.
- Golden-image harness: capture run mode, committed PNG goldens, tolerance
  policy, diff artifacts, four-target CI jobs; pinned toolchains.
- Headless display-list unit tests (the ADR 0019 record/assert gate).

**Non-Goals:**

- Sorting that reorders draws in F2 (see D3 — playback is record order;
  the stable-sort machinery lands with it).
- Premultiplied alpha, MSAA, scissor/viewport API, per-quad `origin`/`pivot`
  options (center pivot only in F2), non-uniform per-quad scale.
- `loadImage` from disk (F6), render targets (F5), hook registration
  (separate runtime change).
- ADR 0019's pose-after-draw strictness: re-targeted to F7 — `poseMesh`
  does not exist in F2, so there is nothing to be strict about.

## Decisions

### D1 — Camera and quad transforms compose at record time

Each record stores the **final composed 2D affine transform** (6 floats) of
the quad: `view(frame → world) ∘ model(quad → world)` computed at record
time from the camera state active at the call. Playback only multiplies
vertices and maps frame → target pixels (stretch: scale `frameW×frameH` to
the surface, y flipped into NDC per the top-left y-down convention — the
ortho matrix handles the flip once).

- View: `T(center) · R(rotation) · S(zoom) · T(−camera_xy)` where `center`
  is the frame center and `camera_xy` the look-at point — this makes zoom
  and rotation pivot on the frame center by construction.
- Model: `T(x + w/2, y + h/2) · R(rotation) · S(scale) · T(−w/2, −h/2)` —
  placement stays corner-anchored while rotation/scale pivot on the center.
- Angles are degrees, converted to radians at record time (radians never
  cross the JS boundary); positive = clockwise in the y-down frame.

*Rejected:* applying the camera at playback from current engine state —
violates ADR 0019's value-snapshot rule and makes recorded draws observe
later camera calls. *Rejected:* storing separate camera id + quad params
and composing at playback — same late-binding problem with extra state.

### D2 — Blend modes as three canned pipeline variants

`alpha` = `src·sa + dst·(1−sa)`; `additive` = `dst + src·sa`;
`subtractive` = `dst − src·sa`, clamped. Straight (non-premultiplied)
alpha. Sokol blend state is immutable per pipeline, so the 2D pipeline
exists in exactly three canned variants (ADR 0015: engine-owned, never
script-visible); the record's blend byte selects the variant at playback.
Mode is **not** a sort key (D3) — with record-order playback no blending
anomaly can arise from reordering.

### D3 — Playback order is record order in F2 (sort key = record index)

ADR 0019 mandates that *if* the renderer reorders, it must be a stable sort
by an explicit key. F2 ships the record arena, the stable-sort
infrastructure, and the key field — but plays back in record order
(key = record index), batching only consecutive same-texture runs. Rationale:
with alpha blending, any texture-based reordering can visibly break
painter's order for overlapping quads; a script-visible `layer` option
would fix that but is speculative API without a driving use case. The sort
field keeps reordering available to F3+ (depth-sorting opaque geometry)
without an API break.

*Rejected:* auto-key on texture handle — incorrect painter's order for
overlapping translucent sprites with different textures, exactly the case
2D games draw constantly. *Rejected:* explicit `layer` option now — no
current user; can be added additively later.

### D4 — Record arena budget: 16 MiB hard cap, overflow throws

Records are ~96 bytes (affine 6×f32, sourceRect 4×f32, tint 4×f32, texture
handle, blend byte, flags), allocated from the frame-transient arena (ADR
0019) rewound at frame end. Hard cap 16 MiB ≈ 170k records — far above any
real 2D frame, reachable by a test loop in milliseconds. Exceeding it
throws `RangeError` ("display list budget exceeded"), which surfaces as a
non-zero exit per the error conventions.

*Rejected:* unbounded growth — turns a script bug into an OOM crash and
undermines the ADR 0012 "native cost is bounded and visible" story.
*Rejected:* silent drop of excess records — unpredictable rendering is the
worst way to learn about a budget.

### D5 — White texture: engine-owned Texture instance

`efx.whiteTexture` is a real Texture object created from an internal 1×1
RGBA8 image at startup — no special-casing in the draw path (the pipeline
always has a texture bound; the vertex color path of F3+ reuses the same
trick). It is not script-owned: `destroy()` throws `TypeError`, the GC
finalizer ignores it, and teardown releases it. Its native size counts
once at init. This keeps `drawQuad` single-purpose (always textured; color
is always a tint) — the alternative `drawRect`/`drawQuad` split would
duplicate the rotation/scale/sourceRect-adjacent option surface for no
capability gain.

### D6 — sourceRect validation: floats allowed, bounds enforced

`sourceRect` components are floats (sub-pixel sampling with linear
filtering is legitimate), validated against the texture's pixel dimensions
(known at upload); any component placing the rect outside `[0, w]×[0, h]`
throws `RangeError`. Omitted = full texture. The renderer converts
frame-pixel rects to normalized sampling coordinates internally — scripts
never see normalized UVs in F2's API.

### D7 — Capture run mode and readback

New player run mode: `efx --capture-frame <N> --capture-output <file>
<resource-root>` — boots the resource root (full `main.js` semantics),
renders N frames, reads back the framebuffer after frame N's present, writes
a PNG, exits 0. Readback is implemented in the platform layer per backend:
GL `glReadPixels` (Linux/macOS), D3D11 staging-texture copy + map
(Windows), WebGL2 `gl.readPixels` into MEMFS (Emscripten). Frames for
golden tests run at a fixed 640×480 window so the default camera's frame
equals the golden's virtual frame. The capture path is test-only
infrastructure — not part of the script-facing API.

*Alternative considered:* an `efx.readPixels` script API — rejected: it is
not in vision.md, would be the only bulk-data API on the namespace, and
capture must also work with scripts that never call it.

### D8 — Golden images: PNG via vendored stb; tolerance ±2/255, ≥99.5% pixels

`stb_image_write` encodes captures; `stb_image` decodes goldens for
comparison (both single-header, vendored pinned snapshots per ADR 0006;
PNG is lossless, diffable, and viewable in any review). Tolerance policy
(settled per the roadmap's F2 assignment; normative in the verification
spec): per-pixel pass = every channel Δ ≤ 2/255; frame pass = ≥ 99.5% of
pixels pass. Failures write `{test}-actual.png` and `{test}-diff.png`
(failing pixels highlighted). Rationale: the four backends (GLCORE,
D3D11, Metal, GLES3/WebGL2) differ in rasterizer rounding and gradient
precision; exact-equality goldens would fail on legitimate floating-point
differences, while a tolerance absorbs them and still catches real
regressions (a wrong transform or color moves pixels far past 2/255).

*Alternative considered:* exact comparison + per-backend goldens — four
golden sets to maintain for no additional defect detection; rejected.

### D9 — CI determinism: software rasterizers + pinned toolchains

- Linux: `xvfb-run` + Mesa llvmpipe (workflow installs the mesa GL
  packages; no GPU on runners anyway).
- Windows: D3D11 falls back to WARP on GPU-less runners — the deterministic
  software path; no extra setup.
- macOS: GL on the pinned runner image (`macos-14`); covered by tolerance.
- Emscripten: emsdk pinned to an exact version in `ci.yml` (settles the
  deferred emsdk-pinning decision); the golden job runs the capture build
  in headless Chrome pinned to a fixed major version with SwiftShader
  (`--use-gl=angle --use-angle=swiftshader`), driving the page and fetching
  the capture via a small Node script — Node remains test-only
  infrastructure (ADR 0008).
- Goldens regenerate only via an explicit, documented invocation; a run
  mode flag (`--capture-output` on a committed golden scene) is that
  invocation.

### D10 — Display-list unit tests link `src/render` without a device

`tests/` gains C tests that include the render module's record API directly
(no sokol context): record a known sequence through the internal record
path, assert on record order/values/keys, and exercise budget overflow.
These run in the existing ctest suite on all four targets — the headless
record/assert gate ADR 0019 names. The JS-side smoke suite additionally
covers the API surface (`drawQuad` argument validation via thrown errors)
through `--script` mode with exit-code assertions, extending the F1
convention.

## Risks / Trade-offs

- [Backend readback complexity (D3D11 staging, Metal absence in F2 is fine
  — macOS uses GL)] → the capture path is small and isolated in the
  platform layer; an early spike task proves each backend before the
  harness depends on it.
- [macOS runner GL variance may exceed tolerance] → tolerance absorbs
  rounding; if a scene still fails, the scene is simplified (avoid
  heavy-gradient quads in goldens) rather than loosening global tolerance.
- [SwiftShader WebGL drift between Chrome versions] → Chrome pinned by
  major version in the workflow; pin is a one-line bump treated like the
  emsdk pin.
- [16 MiB budget may constrain future heavier records] → the cap is a
  named constant, revised by a later change if record payloads grow (F5
  render-target draws); overflow throws loudly, never silently corrupts.
- [Record-order playback leaves batching on the table] → consecutive-run
  batching covers the common sprite-sheet case; real reordering arrives
  with F3's opaque-geometry sort where it is safe.

## Migration Plan

Not applicable — additive engine capability plus a reshaped *provisional*
contract (the `drawQuad` change breaks no shipped script; F1 examples use
no drawing). The docs/js-api.md F2 section is rewritten and F5's
provisional sample refreshed in the same change as required by the js-api
spec.

## Open Questions

None — the deferred items assigned to F2 (tolerance, CI determinism, emsdk
pinning, ADR 0019's sort-key and budget questions) are settled above;
exact CI workflow wiring (package names, Chrome CDP script details) is
verified during apply per D9's spike task.

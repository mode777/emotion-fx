# Proposal

**Roadmap position:** Implements milestone **F2 (2D layer)** from
`openspec/specs/feature-roadmap`. The predecessor gate (F1) has passed on all
four targets, so this proposal is in roadmap order.

## Why

F1 proved the four-platform build and the JS/C boundary, but nothing draws
yet. F2 delivers the first rendering layer — quads, ortho camera, textures,
blending — plus the two architecture pieces the roadmap explicitly stages
here for risk retirement: the re-orderable display list (ADR 0019) and the
golden-image verification harness that every later milestone's gate depends
on. F2 also settles the roadmap-deferred decisions assigned to this
milestone: golden-image tolerance and CI rendering determinism (including
emsdk pinning).

## What Changes

- **2D projection frame & camera (`setCamera2D`)** — sets up a virtual pixel
  frame (e.g. `frame: [640, 480]`); all 2D draw coordinates are frame pixels.
  `x`/`y` name the world point shown at the frame center; `zoom` and
  `rotation` stack on top and pivot on the frame center. Origin is top-left,
  y-down. The frame maps to the active target with a **stretch** policy
  (no letterboxing). Default camera (no `setCamera2D` call): frame equals the
  current window size. Applies to draws recorded after the call (ADR 0019
  value-snapshot semantics).
- **Quad drawing (`drawQuad`)** — **BREAKING vs the provisional contract**:
  `texture` becomes a required 5th argument
  (`drawQuad(x, y, w, h, texture, opts?)`), and new options arrive:
  `sourceRect` (texture-pixel rect, replaces normalized `uv`; omitted = full
  texture; out-of-bounds throws), `rotation` (degrees, clockwise, pivot =
  quad center), `scale` (uniform, pivot = quad center), `color` tint
  (default white). Solid-color rects use an engine-owned 1×1 white texture
  exposed as `efx.whiteTexture` (not script-destroyable).
- **Texture/image resources** — `createImageData` (RGBA8) and
  `createTexture` (CPU→GPU upload), per the provisional contract; both are
  opaque native-backed classes per ADR 0011/0013.
- **Blending** — `setBlendMode`: `alpha` (default) | `additive` |
  `subtractive`, recorded per draw.
- **Display list** — record → playback between the API and the renderer,
  per ADR 0019 (frame-transient record arena, stable-sort reordering,
  handle-referenced textures). F2 settles the open items ADR 0019 deferred:
  sort-key UX, arena budget policy; headless display-list unit tests are the
  record/assert gate.
- **Golden-image harness (first-class deliverable)** — player frame-capture
  run mode, committed PNG goldens, pixel-diff with a settled tolerance
  policy, diff artifact on failure, running on all four targets in CI;
  settles CI rendering determinism (software rasterizer strategy, pinned
  emsdk/browser versions).
- **New vendored dependency: stb (`stb_image`, `stb_image_write`)** for
  golden-image PNG read/write (evaluation below), under the ADR 0006
  pinned-snapshot policy.
- **Docs** — `docs/js-api.md` F2 section rewritten to the settled contract in
  the same change (including provisional F5 samples that use the old
  `drawQuad` shape); AGENTS.md status table updated.

**Dependency evaluation (roadmap requirement — F2 harness is a first
evaluation customer):** candidates for golden-image image I/O: (a)
**stb_image + stb_image_write** — chosen: MIT/public-domain dual license,
two single headers that vendor cleanly as pinned snapshots (ADR 0006),
compiled and proven on all four targets incl. Emscripten, C11-compatible;
PNG goldens are lossless, viewable, and diffable; `stb_image` is reused by
F6's `loadImage`. (b) in-tree BMP/TGA writer+reader — no dependency, but a
hand-rolled parser to maintain for no capability gain. (c) libpng — heavy,
drags in zlib. Selection: (a).

Non-goals: hook registration (`registerUpdateHook`/`registerRenderHook` —
separate runtime change, deadline "F2 at the latest" per ADR 0016); 3D/math
(F3); lighting (F4); render targets/post FX (F5); zip/`loadImage` from disk
and glTF (F6); skinning (F7 — including ADR 0019's pose-after-draw
strictness question, which cannot apply until `poseMesh` exists and is
decided at F7); text/high-level layer (F8); input/audio (no spec); no
script-visible sort-`layer` option (revisit additively if a use case
appears); no letterbox/fit scale modes (stretch only in F2).

## Capabilities

### New Capabilities

- `2d-layer`: 2D drawing behavior — projection frame & camera transform
  semantics, quad drawing (placement, pivot, sourceRect, tint), texture and
  image resources incl. the engine-owned white texture, blending modes, and
  display-list record→playback behavior.

### Modified Capabilities

- `verification`: adds the golden-image harness requirements (frame capture,
  committed goldens, tolerance policy, four-target gate, failure reporting
  with diff artifact) and the headless display-list unit-test gate.

The `js-api` capability needs no requirement delta: its requirements are
meta-level (namespace, layers, resource classification, reference
document). The F2 API surface is behavior-pinned by `2d-layer`, and the
existing js-api requirement "milestone change updates the reference"
already mandates the `docs/js-api.md` update in this change. Texture and
ImageData are already classified (native-backed classes, delivered F2); the
white texture is an engine-owned `Texture` *instance*, not a new class.

## Impact

- **Code:** new `src/render/` module (display list, 2D canned shader
  pipeline, white texture) behind the ADR 0003 module walls; `src/api/api.c`
  gains the F2 bindings; `src/platform` + `src/player` gain the
  frame-capture run mode; examples gain a golden-test resource root.
- **APIs:** F2 section of `docs/js-api.md` moves from provisional to
  current with the settled signatures (same-change update required by the
  js-api spec); `drawQuad` reshapes the provisional contract (texture
  required, `sourceRect` replaces `uv`).
- **Dependencies:** stb (`stb_image`, `stb_image_write`) vendored as pinned
  snapshots; evaluation above satisfies the roadmap's proposal-time
  requirement.
- **CI:** the four-target gate gains golden-image jobs; emsdk and browser
  versions pinned (settles the deferred CI-determinism decision).
- **Decisions:** new ADR `docs/decisions/0020` — golden-image verification
  & CI rendering determinism (tolerance policy, capture strategy, toolchain
  pinning); ADR 0019's deferred open items settled in this change's design.

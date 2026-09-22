# 0020 — Golden images: PNG via vendored stb, ±2/255 tolerance with a
# 0.5% pixel exemption, software rasterizers and pinned toolchains in CI

Status: Accepted (2026-09, change `f2-2d-layer`)

Supports: vision.md — cross-platform compatibility; roadmap — "golden-image
pixel-diff within tolerance" as the F2+ verification gate.

## Context

F2 introduces the golden-image harness every later rendering milestone's
gate depends on. Rendering is backend-plural (GLCORE on Linux, D3D11 on
Windows, Metal on macOS, GLES3/WebGL2 on Emscripten) and CI runners are
GPU-less, so the harness must survive (a) cross-backend floating-point
differences in rasterization and (b) runner-side software rendering. The
roadmap assigned this tolerance/determinism decision to F2. Full process
record: `openspec/changes/archive/2026-09-22-f2-2d-layer/` (design D7–D9).

## Decision

- **Format and I/O:** captures and committed goldens are PNG, encoded with
  vendored `stb_image_write` and decoded with vendored `stb_image`
  (`vendor/stb/`, pinned snapshots per ADR 0006). Goldens live at
  `tests/goldens/<scene>/golden.png`.
- **Capture:** `player --capture-frame <N> --capture-output <file>
  <resource-root>` renders N frames at a fixed 640×480 window and writes
  the frame-N readback. Readback is per-backend platform code (GL
  `glReadPixels`, D3D11 staging copy, Metal blit of an injected Managed
  texture). Capture is test infrastructure, never a script API.
- **Tolerance (normative in the `verification` spec):** a pixel passes when
  every RGB channel differs by ≤ 2 of 255; a frame passes when ≥ 99.5% of
  pixels pass. Failures write `<scene>-actual.png` and `<scene>-diff.png`.
- **Determinism:** captures are only compared under software/pinned
  rendering: Linux CI uses Xvfb + Mesa llvmpipe, Windows uses D3D11 WARP,
  macOS uses the pinned runner image's GL/Metal, Emscripten uses pinned
  headless Chrome with SwiftShader; emsdk is pinned to an exact version in
  `ci.yml`. Goldens regenerate only via the documented invocation when
  intended output changes.

## Consequences

- Every rendering milestone F3–F8 gets its gate by adding scenes, not by
  re-deciding verification; `tests/run_golden.cmake` + `efx_imgdiff` are
  the fixed harness surface.
- Legitimate cross-backend rounding is absorbed (≤2/255 per channel);
  real regressions (wrong transforms, colors, blend equations) move
  pixels far past tolerance and fail. Sub-2/255 visual regressions are
  accepted noise.
- Toolchain bumps are deliberate acts: a runner/toolchain update that
  fails goldens means the pin moves with a reviewed regen, never a silent
  re-baseline.
- Local development without a window system runs the suite with
  `-DEFX_BUILD_GOLDEN_TESTS=OFF` (the default); CI turns it on.

## Rejected alternatives

- **Exact comparison + per-backend goldens:** four golden sets to maintain
  for no extra defect detection; backend noise would flake constantly.
- **BMP/TGA with in-tree readers:** no dependency, but a hand-rolled
  parser to maintain; PNG is viewable and diffable in every review tool.
- **libpng:** heavy and drags in zlib for one comparison tool.
- **Per-frame script API readPixels:** a bulk-data consumer API not in
  vision.md; capture must work for scripts that never call it.

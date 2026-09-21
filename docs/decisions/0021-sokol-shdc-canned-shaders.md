# 0021 — Canned shaders come from sokol-shdc; one GLSL source, no hand-written per-backend flavors

Status: Accepted (2026-09, change `f2a-sokol-shdc`)

Supports: vision.md — cross-platform compatibility; roadmap — canned-shader
strategy "settled at F4 at the latest" (settled early, with cause).

## Context

F2's 2D pipeline shipped with three hand-written shader sources — GLSL ES
(GLCORE/GLES3), HLSL (D3D11), MSL (Metal) — and a manually assembled
`sg_shader_desc`. GL rendered pixel-correctly; D3D11 and Metal silently
drew no geometry. Debugging hand-maintained backend flavors through CI
roundtrips is exactly the failure mode sokol's shader cross-compiler
(sokol-shdc) exists to prevent: one annotated GLSL source, compiled at
build time into a C header carrying correct per-backend sources, entry
points, attribute semantics and resource bindings. Full process record:
`openspec/changes/f2a-sokol-shdc/`.

## Decision

All engine canned shaders are written **once, in sokol GLSL conventions**,
in `shaders/*.glsl`, and compiled with **vendored sokol-shdc** into
committed C headers at build time; `sg_shader_desc` wiring for these
shaders comes exclusively from the generated headers. Hand-written shader
sources or hand-assembled desc wiring for backend flavors MUST NOT be
added. The tool is vendored as a pinned source snapshot and built as a
CMake host tool (build-time only — never linked into the player), keeping
builds offline per ADR 0006.

## Consequences

- Backend shader defects are fixed in one place and re-emitted; a golden
  diff, not a silent no-op, is the failure signature.
- F4's lighting shaders (Phong channels, maps) reuse this pipeline without
  new per-backend work.
- The tool pin is a deliberate, reviewed bump; the generated headers are
  committed and CI verifies regeneration produces no drift.
- Emscripten keeps compiling from the same single source (GLES3 flavor),
  though its verification was explicitly out of scope for the adopting
  change (`f2a-sokol-shdc`) and is `f2b-web-native-runtime`'s to gate.

## Rejected alternatives

- **Hand-written per-backend sources** (F2's original approach): produced
  the D3D11/Metal silent no-op defect; every new shader multiplies the
  maintenance surface by the backend count.
- **Prebuilt sokol-tools-bin binaries**: binary blobs in-repo and a
  download/trust step conflict with the vendored-source snapshot policy
  (ADR 0006).
- **Runtime shader compilation from GLSL**: contradicts the fixed-function
  consumer contract (ADR 0015) and adds a runtime compiler dependency no
  target needs.

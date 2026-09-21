# Design

## Context

`f2-2d-layer` shipped the 2D canned pipeline with three hand-written shader
sources (GLSL ES for GLCORE/GLES3, HLSL for D3D11, MSL for Metal) and a
manually assembled `sg_shader_desc`. GL renders correctly (Linux goldens
pass pixel-exact); D3D11 and Metal render the clear but draw no geometry.
Fixing hand-maintained per-backend shader flavors per CI roundtrip is the
wrong investment — sokol's shader cross-compiler (sokol-shdc) exists to
generate exactly this artifact class from one GLSL source.

Decision record: `docs/decisions/0021-sokol-shdc-canned-shaders.md`
(written with this proposal).

## Goals / Non-Goals

**Goals:**

- One shader source (`shaders/quad.glsl`) for all platforms, compiled to a
  generated C header at build time via vendored sokol-shdc.
- D3D11 and Metal quad rendering correct — native golden gates pass.
- Builds stay fully offline and deterministic (ADR 0006 policy).

**Non-Goals:**

- Any Emscripten work or verification (f2b's scope; the GLES3 flavor shdc
  emits compiles as-is but is untested and ungated here).
- New shader features (lighting arrives in F4 on this same pipeline).
- A consumer-visible change of any kind.

## Decisions

### D1 — Compile sokol-shdc from a vendored source snapshot as a CMake host tool

The sokol-tools sources are vendored under `vendor/sokol-tools/` (pinned
snapshot, recorded in `vendor/README.md` per ADR 0006) and built as a
CMake host-tool target (`shdc`), then invoked via `add_custom_command` to
generate `shaders/quad.h` from `shaders/quad.glsl` at build time. The
generated header is committed so ordinary builds do not need to rebuild
the tool, and a CMake target regenerates it when the source changes.

*Rejected:* prebuilt binaries from sokol-tools-bin — binary blobs
in-repo and a download/trust step conflict with the vendored-source
policy. *Rejected:* keeping hand-written sources — that is the defect
being fixed.

### D2 — Single GLSL source, sokol-shdc emits all native backends

`shaders/quad.glsl` carries the existing quad vertex/fragment stages
(`@vs quad_vs` / `@fs quad_fs`, `@program quad quad_vs quad_fs`) in sokol
GLSL conventions. shdc emits GLSL330 (GLCORE), HLSL (SM4, D3D11) and MSL
(Metal) with correct attribute semantics, entry points and bindings —
replacing `VS_SRC`/`FS_SRC` and the manual `sg_shader_desc` attribute,
sampler and texture-sampler-pair wiring in `src/platform/pipeline.c`
(which become the generated `quad` shader include + `#include
"shaders/quad.h"`). The GLES3 flavor emitted by shdc keeps the
Emscripten build compiling unchanged (out of scope, unverified here).

### D3 — Golden gates are the acceptance test

Acceptance = native golden jobs green: Windows (D3D11/WARP), macOS
(Metal), Linux (GLCORE/llvmpipe) — same committed goldens, same tolerance
policy (ADR 0020). No new tests; the existing gates are the point.

## Risks / Trade-offs

- [shdc mis-compiles for a backend] → the generated header is committed
  and reviewable; regressions appear as golden diffs, not silent build
  drift; the tool pin is a one-line bump.
- [CMake host-tool build adds configure time] → shdc is a single small C
  program; building it is seconds and cached.
- [Generated-header-in-repo drift] → the custom command regenerates on
  source change; CI builds verify no drift (regenerate + `git diff
  --exit-code` in the Linux job).

## Migration Plan

Land shdc integration + generated header in one commit; native golden
jobs flip green or the change is not merged. No rollback complexity —
revert restores the hand-written pipeline.

## Open Questions

None.

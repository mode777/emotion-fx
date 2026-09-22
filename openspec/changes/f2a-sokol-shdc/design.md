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

### D1 — Pinned sokol-shdc binary generates the committed header (apply resolution)

> **Resolved during apply:** D1 was originally written as "compile
> sokol-shdc from a vendored source snapshot as a CMake host tool".
> Inspection showed sokol-tools has **no CMake build** and pulls an
> 8-submodule dependency graph (glslang, SPIRV-Tools/Cross, tint, fmt,
> ...) — impractical for a generation-time-only tool. Resolution: the
> compiler is the **pinned sokol-tools-bin revision**
> (`11d0cf678105d614d675e6d9bd2aaf3eeff12f8c`, recorded in
> `vendor/README.md` with the regeneration procedure); the generated
> `shaders/quad.h` is committed, so the player build never needs the
> tool and stays offline. ADR 0021 was amended accordingly (its
> "rejected alternatives" entry now records the source-graph option as
> the rejected one). The decision's invariant is unchanged: one GLSL
> source, generated header is the only shader artifact in the build.

`shaders/quad.h` is generated from `shaders/quad.glsl` by the pinned
sokol-shdc (`-f sokol_impl`, slangs `glsl410:glsl300es:hlsl4:metal_macos`)
and committed; regeneration follows the documented procedure in
`vendor/README.md`.

*Rejected:* keeping hand-written sources — that is the defect being
fixed. *Rejected:* source-graph vendoring — see above.

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

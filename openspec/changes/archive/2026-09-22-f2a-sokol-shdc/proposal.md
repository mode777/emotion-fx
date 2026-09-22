# Proposal

> **STATUS: PROPOSED.** Stacks on `f2-2d-layer` (incomplete/blocked) and
> precedes `f2b-web-native-runtime`. Together the three changes complete
> milestone F2.

**Roadmap position:** Implements milestone **F2 (2D layer)** — the canned
shader strategy that F2's rendering gate depends on. The roadmap assigns
the canned-shader strategy decision to "F4 at the latest"; this change
settles it early, with cause (see Why).

## Why

`f2-2d-layer` shipped the 2D pipeline with hand-written per-backend shader
sources (GLSL ES, HLSL, MSL) and a manually assembled `sg_shader_desc`.
That works on GLCORE (Linux goldens pass) and GLES3, but renders nothing
for shader-driven draws on D3D11 (Windows) and Metal (macOS) — the desktop
golden gates cannot pass. Hand-maintaining N backend flavors of the same
shader, plus the desc wiring, is precisely the error class sokol's shader
cross-compiler exists to eliminate; debugging it per-backend via CI
roundtrips is the wrong investment when the tool provides a single-source
workflow.

## What Changes

- **Adopt sokol-shdc** as the build-time shader compiler: one annotated
  GLSL source file (`shaders/quad.glsl`, `@vs`/`@fs` blocks) is the single
  shader source for all platforms; the generated C header replaces the
  hand-written VS_SRC/FS_SRC strings and the manual `sg_shader_desc`
  wiring in `src/platform/pipeline.c`.
- Vendor the sokol-shdc tool (same upstream project as the already-vendored
  sokol headers, pinned snapshot per ADR 0006) and integrate it as a CMake
  host-side build step; builds remain fully offline.
- **Emscripten is out of scope for this change**: the GLES3 flavor shdc
  emits compiles as-is, but no Emscripten work, verification, or CI gating
  happens here — the `golden tests (emscripten)` job is already red from
  `f2-2d-layer`'s blocker and is `f2b-web-native-runtime`'s to resolve.
- No consumer-API change; no spec deltas (implementation + docs only —
  `skip_specs: true`). The existing `verification` requirements
  (golden gates on all four targets) are what this change lets the native
  jobs satisfy.
- **ADR**: `docs/decisions/0021-sokol-shdc-canned-shaders.md` — written
  with this proposal (the decision is made here); see the docs section
  below.

**Dependency evaluation (roadmap rule):** the only new tool is
sokol-shdc. Candidates: (a) **compile from a pinned source snapshot** of
sokol-tools via a CMake host-tool target — chosen: deterministic, fully
offline, no binary blobs in-repo, builds on all four CI hosts (it is a
build-time host tool, never linked into the player, so the C11/target
constraints do not apply); MIT license, same upstream and pinning policy
as the vendored sokol headers (ADR 0006). (b) prebuilt binaries from the
sokol-tools-bin release — rejected: binary blobs in-repo conflict with the
vendored-source snapshot policy and add a download/trust step.

Non-goals: Emscripten/web verification (f2b); the web GC crash (f2b);
consumer-API or spec changes; new shader features beyond the existing quad
shader (the F4 lighting shaders will reuse this pipeline).

## Capabilities

None — `skip_specs: true`. Implementation, build tooling, and docs only;
all behavior requirements already exist in `verification` and `2d-layer`.

### Modified Capabilities

None.

## Impact

- **Code:** `shaders/quad.glsl` (new single source); generated header
  (committed output of the build step) replaces `VS_SRC`/`FS_SRC` and the
  manual desc wiring in `src/platform/pipeline.c`; CMake gains the shdc
  host-tool target and a custom command.
- **Docs:** `docs/decisions/0021` (new, with this proposal); AGENTS.md
  canned-shader note updated; `f2-2d-layer/design.md` gets an addendum
  pointing at the supersession of D-section shader notes.
- **Verification:** native golden gates (Windows D3D11, macOS Metal,
  Linux GLCORE) become passable; the F2 gate remains blocked on the web
  job until `f2b-web-native-runtime` lands.

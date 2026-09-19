# Design

## Context

The repository has no source, build files, or tests — only `vision.md` and the
OpenSpec scaffolding (see proposal.md - Why). The roadmap is therefore a
planning artifact about future planning: its "implementation" is the roadmap's
existence, ordering, and documentation. All engine architecture below is
inherited from `vision.md` and constrains how the ladder is cut; it is not
built by this change.

## Goals / Non-Goals

**Goals:**
- A fixed, risk-ordered milestone decomposition of `vision.md` that future
  `/opsx-propose` runs slot into without re-litigating sequencing.
- A shared, engine-wide verification strategy defined once (F2 harness) instead
  of per-change.
- Durable documentation of the ladder in `AGENTS.md`.

**Non-Goals:**
- Implementing any milestone.
- Settling the deferred technical decisions (canned-shader strategy, math
  library, QuickJS flavor, asset format) — this change only assigns each
  decision a latest-settling milestone.
- Defining capabilities of individual milestones (each future milestone change
  writes its own capability specs).
- Input handling and audio — absent from `vision.md`; explicitly out of scope
  until the user adds them to the vision.

## Decisions

### D1: Eight milestones, cut by risk retirement rather than by feature list
The ladder leads with the two highest-uncertainty axes — the 4-platform build
matrix (F1) and the JS/C boundary with the display-list architecture (F1–F2) —
before any attractive rendering work. Alternative considered: feature-by-feature
order from the vision bullet list (lights first, platform last). Rejected:
platform rot and an untestable rendering core would compound silently; a
walking skeleton with `--script` mode makes every later change cheaply
verifiable.

### D2: Verification strategy is itself a milestone deliverable (F2)
Golden-image pixel-diff (readback + tolerance comparison) is introduced in F2
and reused by F3–F8. Non-visual logic uses unit tests; F4 lighting and F7
skinning get CPU reference implementations, which the fixed-function constraint
makes tractable. Alternative: manual visual checks per milestone. Rejected:
development is AI-agent-driven (per `vision.md`), so verification must be
automatable.

### D3: F4 (Phong/lighting) is split into F4a/F4b
F4a delivers lights + solid/vertex-color Phong; F4b adds per-channel maps and
alpha masks. The split isolates the canned-shader strategy decision (mega-shader
vs build-time permutations — the costliest retrofittable decision in the
engine) to the smallest change that must make it, informed by F2/F3 experience.

### D4: Asset import deferred to F6; procedural primitives in F3
F3 uses generated primitives so the mesh pipeline is exercisable without
committing to an asset format. The format decision (glTF subset via cgltf vs
custom binary) is assigned to F6 where it is actually needed. Alternative:
glTF import in F3. Rejected: drags the format decision across the F3 gate and
grows F3 beyond one milestone.

### D5: CPU skinning for F7
Skinning runs on CPU into a dynamic mesh slot, which sidesteps putting
animation logic into canned shaders and matches the fixed-function constraint.
Alternative: shader-based skinning via permutation variants. Rejected as
default: more shader-permutation surface for no era-appropriate benefit;
may be revisited inside F7 if CPU cost is prohibitive.

### D6: Roadmap lives in the `feature-roadmap` capability + `AGENTS.md`
The ordering rules are spec'd (archivable, enforceable) and mirrored as a
short section in `AGENTS.md` (session-visible). `AGENTS.md` references the
spec rather than duplicating scenarios.

## Risks / Trade-offs

- [Roadmap proven wrong by F1/F2 findings (e.g., platform matrix infeasible)]
  → F1 is deliberately the first gate; if it fails, the ladder halts by design
  and this roadmap is revised via a new change rather than silently bypassed.
- [Golden-image tests are brittle across GPUs/drivers] → Define tolerance
  thresholds in F2; investigate software-GL determinism for CI at F2 time.
  Emscripten browser verification may need headless-browser tooling or stays
  manual — F2 must state which.
- [Eight milestones is optimistic; scope creep within milestones] → Each
  milestone is exactly one OpenSpec change with its own gate; anything that
  doesn't fit is split, not absorbed.
- [Vision gaps (input, audio) surface mid-ladder] → Out of scope until the
  vision changes; if added, they enter as F9+ proposals and must respect the
  existing ladder gates.

## Migration Plan

Not applicable — no code, no deployment. Adoption: merge this change, update
`AGENTS.md` in the same change, then future feature proposals follow
`/opsx-propose` starting with `add-player-skeleton` (F1).

## Open Questions

Deferrable without changing specs or the task breakdown:
- Exact toolchain versions (CMake, emsdk, compilers) — settled inside F1.
- QuickJS flavor (bellard vs quickjs-ng) and math library (vendored vs
  hand-rolled) — settled inside F1; rayjs informs the former.
- Golden-image tolerance mechanics and CI determinism approach — settled
  inside F2.
- Asset format (glTF subset vs custom) — settled inside F6.

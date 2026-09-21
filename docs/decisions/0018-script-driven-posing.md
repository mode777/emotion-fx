# 0018 — Script-driven posing; no engine playback state

Status: Accepted (2026-09, change `js-api-reference`)
Amends: 0017 (implicit rig payload stands; its playback APIs are replaced)

## Context

ADR 0017 removed Skeleton/Animation resources but kept engine-driven
playback — `playAnimation`/`pauseAnimation`/`blendAnimations` — with hidden
state inside the Mesh: current clip(s), advancing time, blend weights, and
the posed vertex buffer. Review questioned whether the trio earns its place,
or whether scripts should drive animation clocks themselves in the update
loop. The engine had just moved to explicit hook registration (0016) and
per-draw draw options (0017); hidden time accumulation inside native meshes
is the last piece of implicit runtime behavior — and the F7 verification
gate ("FK joint-transform tests vs CPU reference") favors a stateless,
directly testable posing primitive.

## Decision

- **One stateless posing call replaces the playback trio**:
  `efx.poseMesh(mesh, pose)` — `pose` is a single sample
  `{ clip, time, weight? }` or an array of samples (multi-clip blend).
  `time` is seconds into the clip, wrapped modulo clip length engine-side;
  weights are normalized engine-side (negative weights throw). The call
  CPU-poses in place into the Mesh's posed buffer.
- **The script owns the clock** in its update hook: looping, speed,
  freezing, reverse, cross-fades, and procedural time are plain script code
  (e.g. `k = Math.min(1, t / 2)` driving two sample weights).
  `skinned: true` on `drawMesh` continues to select the posed buffer; a
  never-posed mesh draws its bind pose.
- **No playback state exists engine-side** — no current clip, no
  auto-advance, no pause flag. `playAnimation`/`pauseAnimation`/
  `blendAnimations` are dropped from the catalog.
- **Stateful playback may return as pure-JS sugar** (an animation-player
  helper over `poseMesh`) in the high-level layer (F8 scope) — the layering
  rule (low/mid in C, high in pure JS, ADR 0011-era design) makes that
  additive without engine changes. The reverse — decomposing hidden C
  playback once shipped — would not be possible.

## Consequences

- F7 catalog: two functions — `poseMesh` and `drawMesh` (with `skinned`).
- The simplest case costs a clock line plus a pose call instead of one
  `playAnimation` — accepted; the future JS helper covers it.
- Posing is deterministic and directly unit-testable: sample in, posed
  vertices out — matching the F7 CPU-reference gate exactly.
- REPL-friendly: pose a mesh at an explicit time and draw it immediately.
- glTF data mapping (0014) and implicit rig payload (0017) are unchanged;
  clips are referenced by name or index (naming rules follow the F6
  asset-format decision).

## Rejected alternatives

- **Engine-driven playback state (0017's trio)**: lost — hidden time/blend
  state, three functions plus future options (transition curves, speeds),
  and reference-testing friction; its convenience is recoverable in pure
  JS at any time.
- **Per-draw clip/time parameters on `drawMesh`**: lost again — simulation
  state does not belong in draw options (0017's buffer-vs-state line);
  would also break one-Mesh-one-pose for multi-draw frames.
- **Strict weight validation (throw unless sum == 1)**: lost — engine-side
  normalization is friendlier for cross-fade code; negative weights still
  throw.

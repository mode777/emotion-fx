# 0010 — Script math is plain JS data; GLM math stays behind the C wall

Status: Accepted (2026-09, change `js-api-reference`)

## Context

F3 must decide how vectors, quaternions, and matrices reach scripts. The
runtime is quickjs-ng (ADR 0002) — an interpreter with no JIT — so math is
the worst-case workload for the JS/C boundary: games issue hundreds of small
math operations per frame per entity, each one a marshalling round trip and
a GC allocation if math lived in C-backed objects. Vision.md's layering rule
(high-level conveniences in pure JS) and memory rules (value-like resources
must not become unmanaged handles) constrain the design, and ADR 0005
already places GLM behind a plain C wrapper for engine-internal math.

## Decision

Math is split by hot path, with plain data as the only thing that crosses
the JS/C boundary:

- **Script-facing math is pure JS**, engine-bundled (`efx.mat4` / `efx.vec3`
  / `efx.quat`): free functions in raymath style over column-major
  `Float32Array`s (mat4 = 16 floats, vec3 = 3, quat = `[x, y, z, w]`),
  with optional `out` parameters for hot loops. No classes, no handles.
- **The boundary contract is plain data.** Math-taking API calls
  (`drawMesh`, `setLight`, camera setters, …) accept column-major
  `Float32Array`/number arrays; the C side converts once per call, at
  display-list record time. The data format is the contract — users may
  substitute any pure-JS math library (e.g. gl-matrix) for `efx.mat4`.
- **GLM stays engine-internal** (ADR 0005): camera matrix construction, CPU
  skinning (F7), and display-list playback transforms. GLM types never
  reach scripts, and per-element loops (per-vertex, per-joint) never cross
  into JS.
- **Escape hatch rule:** when a script math hot spot is measured, fold that
  operation into an existing C call via decomposed inputs (e.g.
  `drawMesh({ mesh, pos, rotY, scale })`) — never by adding C math types.

## Consequences

- Scripts get value-semantics math with zero leak risk: nothing to free,
  GC handles plain arrays, and every math-taking call accepts them without
  accessor boilerplate.
- The interpreter ceiling is accepted by design: script-side numeric loops
  are slow, so anything per-element (skinning, vertex math) MUST live in C.
  F7 CPU skinning is mandatory, not an optimization.
- `efx.mat4` completeness becomes a JS-layer maintenance burden that grows
  with milestones — or users vendor gl-matrix; both are acceptable.
- F3 must verify quickjs-ng typed-array performance is adequate for the
  per-frame math budget (~10³ small ops/frame), and must implement the
  C-side array validation for math-taking calls.
- GLM remains invisible to the script API permanently — `js-api` deltas
  must not expose GLM-backed types.

## Rejected alternatives

- **C-backed math objects** (handles/classes with accessor bindings):
  lost — per-operation marshalling dominates, value-type churn creates GC
  pressure, and GLM types would leak into the script API.
- **Pure-JS math as classes with methods** (three.js style): lost —
  allocation-heavy; it works on V8 only because the JIT performs escape
  analysis, which quickjs cannot.
- **Math only in C, decomposed parameters everywhere**: lost as the sole
  story — game logic needs general vector/matrix arithmetic (movement,
  steering, aim) that decomposed call parameters cannot express; retained
  only as the escape-hatch pattern.

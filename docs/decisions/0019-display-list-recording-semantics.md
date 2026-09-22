# 0019 — Display list records are frame-transient; value-snapshot small state, handle-reference resources

Status: Accepted (2026-09, F2 design exploration — implementation landed in
the archived `f2-2d-layer` change)
Supports: vision.md — "immediate mode api does not mean immediate mode
rendering"

## Context

F2 introduces the re-orderable display list between the immediate-mode
script API and the renderer. Review raised the concern that recording
commands forces the engine to *cache* expensive things — posed vertex
buffers, material state — with performance costs and surprising
late-binding behavior, and asked whether plain immediate drawing would be
better.

That concern is largely pre-defused by decisions already made for other
reasons: materials are plain JS objects, never native resources (0013);
draw calls take per-draw option objects rather than binding global state
(0017's buffer-vs-state line); posing is stateless and writes in place
into the Mesh's posed buffer, with one-mesh-one-pose per frame (0018).
Separately, the JS→C boundary forces every draw API — immediate or not —
to marshal option values into C structs at call time; a live JS object
cannot be read later without GC hazards. The real choice is therefore not
"cache vs no cache" but where the marshalled bytes go and what semantics
they get.

## Decision

The display list is a **frame-transient arena of POD records** with four
recording rules:

- **Frame-transient**: the list is rebuilt (arena rewound) every frame.
  Nothing persists across frames, so there is no cache invalidation
  problem, ever.
- **Value-snapshot small state**: all per-draw scalar/state options
  (transform, color, uv rect, blend mode, flags) are copied into the
  record at call time. Mutating a JS option object after the draw call
  has no effect on it — the same semantics as raylib-style immediate
  APIs.
- **Handle-reference big resources**: Mesh, Texture, and (from F5)
  RenderTarget are referenced by handle in records; their bytes are never
  copied into the list. A skinned draw records the Mesh handle plus the
  `skinned` flag, not vertex data.
- **Exactly one declared late-binding**: the posed vertex buffer. All
  draws of a skinned mesh within a frame see the mesh's final pose for
  that frame — the already-declared one-mesh-one-pose rule (0018),
  recorded here as intentional list semantics rather than an accident of
  playback.

Ordering: playback order is a **stable sort of records by an explicit
sort key** (layer, blend mode, texture, …); records with equal keys keep
record order. Reordering is key-driven and predictable, never heuristic,
and transparent draws keep their painter's order within a layer.

The list is an internal boundary: draw calls never map 1:1 to GPU draws,
and no script-visible API inspects records — but headless tests may
record a frame and assert on the command array without a GPU (this is the
F2 display-list unit-test gate).

## Consequences

- Record cost is ~64–128 bytes per draw in a rewound arena (10k draws ≈
  under 1 MB/frame); the marshalling work is identical to what any
  immediate API must do at the JS→C boundary. Reordering plus quad
  batching then reduces state changes and draw calls below what naive
  immediate would pay.
- One documented behavior delta vs naive immediate expectations:
  `poseMesh` between two draws of the same mesh affects both. F2 had no
  `poseMesh`, so the strictness question was re-targeted to F7 — there is
  nothing to be strict about until posing exists.
- Stable-sort determinism gives a platform-independent draw order, which
  the F2 golden-image harness relies on.
- The three items deferred here were settled by the archived `f2-2d-layer`
  change: the F2 sort key is the record index (playback is record order,
  no reordering yet); the record arena is a 16 MiB hard cap that throws
  `RangeError` on overflow; and the pose-after-draw strictness moved to
  F7 as above.

## Rejected alternatives

- **Pure immediate (no list)**: a hidden batcher reappears anyway (one
  sg_draw per quad is not viable on GL/WebGL), so the copying cost
  returns while reordering, record-and-assert testability, and
  determinism are lost.
- **Late-binding retained list** (records reference live JS objects /
  global state, read at playback): alien semantics for an immediate-mode
  API, GC hazards across the boundary, and the sorter would have to
  evaluate live user state.
- **Retained scene graph**: cache-invalidation complexity contradicting
  the raylib-like immediate philosophy; nothing in vision.md needs it.

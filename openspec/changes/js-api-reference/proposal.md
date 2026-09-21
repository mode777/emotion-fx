# Proposal

**Roadmap position:** Documentation/contract change — it implements no runtime
behavior of any single milestone. It pins down, in advance, the script-facing
API surface that milestones F1–F8 deliver incrementally; every API element the
document defines is tagged with its delivery milestone, so it gates nothing and
violates no predecessor gate. It complements the in-flight `f1-player-skeleton`
change (whose `efx` binding pattern it formalizes) and gives every later
milestone change a stable naming/layering contract to extend instead of
re-inventing.

## Why

vision.md defines the consumer-API rules (layering, resource/memory model,
zero browser/Node dependencies) but no concrete API surface. F1 is nearly done
(24/29 tasks) and established the pattern — a single global `efx` namespace
with `efx.log`, `efx.quit`, `efx.args()` — which the README declares "the
binding pattern all future engine functions follow". F2 will immediately start
adding rendering functions (`drawQuad`, cameras, blending); without a written
API reference there is nothing to keep names, signatures, layering (C vs pure
JS), and the slot/handle resource model consistent across eight milestones of
changes. Writing the reference now, while the surface is tiny, is the cheapest
moment to settle conventions — and each later milestone extends the document
instead of making ad-hoc naming decisions inside its own change.

## What Changes

- Add `docs/js-api.md` — the normative JavaScript API reference for the whole
  engine, derived from vision.md and the F1 implementation:
  - Conventions: single global `efx` namespace, camelCase verb-first naming,
    option-object parameters, degrees/units policy.
  - Lifecycle contract: loading `main.js` is the implicit init (engine
    fully ready before it executes); frame callbacks via explicit, stacking
    `efx.registerUpdateHook`/`efx.registerRenderHook` registration with
    unsubscribe (F1's global `update`/`render` remain as load-time sugar).
  - Full function catalog grouped by milestone F1→F8 (environment/utilities,
    2D drawing, camera, meshes and math, materials and lights, render targets
    and post FX, resource loading, skinning/animation, high-level
    `drawModel`/`drawText`), each entry tagged with its delivery milestone and
    implementation layer (`[C]` low/mid-level, `[JS]` high-level pure JS).
  - Resource & memory model: JS-managed data objects, seven native-backed
    GC-finalized opaque classes (MeshData, ImageData, Skeleton, Animation
    on the CPU; Mesh, Texture, RenderTarget on the GPU — skinning follows
    the glTF data model, ADR 0014 — fully opaque at first, `destroy()`
    only), and slot banks reserved for the fixed light limits —
    fixed-limits table (4 point lights, 1 directional, 1 camera) plus the
    native-memory GC discipline (per ADRs 0011/0012).
  - Error handling and exit-code interaction of API calls.
- Add a new `js-api` capability spec that makes this reference normative: the
  layering rules, namespace rule, dependency rule, memory-model rules, and the
  requirement that every feature change updating the API surface also updates
  `docs/js-api.md` in the same change.

Non-goals: no code changes, no new engine functions implemented, no decision
on deferred milestone internals (asset format stays with F6, canned-shader
strategy with F4, golden-image tolerance with F2). The document records
signatures as contracts-to-implement, not as current behavior, and flags
vision gaps (e.g. input handling is absent from vision.md) as open questions
rather than inventing APIs.

## Capabilities

### New Capabilities

- `js-api`: the script-facing API contract — namespace and naming rules,
  low/mid `[C]` vs high-level `[JS]` layering, lifecycle hooks, resource
  memory model (JS-managed / slots / handles), fixed limits, dependency
  restrictions, and the milestone-tagged API catalog that `docs/js-api.md`
  renders as the developer-facing reference.

### Modified Capabilities

None. `feature-roadmap` is unaffected (this change adds no milestone
behavior); `js-runtime` (in-flight, F1) owns the binding mechanism and stays
as is; `js-api` owns what the functions are.

## Impact

- **Files:** new `docs/js-api.md`; new `openspec/specs/js-api/spec.md` after
  archive. No source, build, or test changes.
- **Future changes:** F2–F8 feature changes gain a normative naming/layering
  target — each will carry a `js-api` delta when it adds or changes API
  functions. Slight overhead per change, in exchange for cross-milestone
  consistency.
- **Docs:** README gains a pointer to the reference; AGENTS.md constraint list
  stays authoritative for design rules (the document references, not
  duplicates, them).
- **Risk:** the catalog forward-declares signatures for unimplemented
  milestones; those sections are explicitly marked provisional so later
  changes may refine signatures via their own `js-api` deltas without the
  document silently drifting.

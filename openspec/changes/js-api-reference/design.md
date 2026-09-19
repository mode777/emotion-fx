# Design

## Context

F1 is 24/29 tasks in and its code already fixes several API facts: engine
functions live on a single global `efx` object (`efx.log`, `efx.quit`,
`efx.args` in `src/runtime/runtime.c`), entry scripts define global
`update`/`render` hooks called with no arguments, and the README declares the
`efx` binding pattern as the template for all future functions. vision.md
supplies the property list (layering, fixed-function pipeline, fixed limits,
memory rules, zero browser/Node dependencies) but no concrete surface. F2
starts adding rendering functions next, so conventions must exist before —
not during — the first rendering change. See proposal.md for motivation.

## Goals / Non-Goals

**Goals:**

- One normative document (`docs/js-api.md`) covering the entire script API
  from F1 through F8, machine-greppable and diff-friendly.
- Settle cross-milestone conventions now: namespace, naming, parameter style,
  units, error style, resource/memory model, slot-vs-handle classification.
- Make every catalog entry traceable to vision.md and to the milestone that
  delivers it, so later changes extend rather than re-decide.
- Keep the document honest: current behavior (F1) visibly separated from
  provisional, to-be-implemented contracts.

**Non-Goals:**

- No code changes; no new functions implemented; no binding mechanism changes
  (owned by the `js-runtime` capability).
- Not settling deferred milestone internals: golden-image tolerance (F2),
  canned-shader strategy (F4), asset format (F6). The document references
  those decisions as pending where they touch API shape.
- Not a tutorial or engine manual — a reference, with minimal examples.

## Decisions

### D1: Single namespace object `efx`, not flat globals
All engine functions — C-implemented and pure-JS high-level — are members of
one global object `efx` (e.g. `efx.drawQuad`, `efx.drawModel`).
**Why:** matches the F1 implementation exactly (zero migration), avoids
polluting global scope that user scripts also use for hooks and state, and
makes the public surface enumerable (`Object.keys(efx)`). **Alternative
rejected:** raylib/rayjs-style flat globals — more idiomatic typing for
raylib users but collides with F1's established pattern and invites name
clashes with user code. Lifecycle hooks remain free globals (`update`,
`render`) because the *script* provides them to the engine; only
engine-provided functions live on `efx`.

### D2: Document location and shape — single `docs/js-api.md`
One markdown file, ordered by milestone (F1→F8 sections), plus convention and
memory-model sections up front. **Why:** greppable, reviewable in PR diffs,
no tooling; the project bans browser/Node deps even for docs tooling in
spirit. **Alternative rejected:** generated docs from JSDoc comments — needs
a toolchain and the C-implemented functions have no JSDoc source of truth.

### D3: Entry format and tag scheme
Each catalog entry is a compact signature sketch plus a short description,
tagged with layer and delivery milestone:

```js
// F2 · C
efx.drawQuad(x, y, w, h, opts?)   // records a colored/textured quad
```

- Layer tag: `C` (low/mid, C-implemented) or `JS` (high-level, pure ES6).
- Milestone tag `F1`…`F8` names the delivering milestone; entries whose
  milestone has not passed its gate carry a `provisional` marker and may be
  reshaped by that milestone's change (via its `js-api` delta).
- Option-object parameters (`opts?`) for anything beyond ~3 scalar args, so
  signatures extend without breaking calls. Scalar-first for hot
  immediate-mode calls (`drawQuad`, `drawMesh`).

### D4: Units and value conventions
Angles in **degrees** (old-school/raylib convention; converted internally —
GLM's radians never leak into the API), time in **seconds** (`update(dt)`),
colors as `[r, g, b, a]` arrays of normalized floats 0..1, positions/sizes in
world units. **Alternative rejected:** 0–255 colors — raylib heritage, but
normalized floats match the fixed-function color path and vertex-color data
without per-call conversion.

### D5: Error style — throw, don't return codes
API functions signal invalid input by throwing standard ES6 errors
(`TypeError`, `RangeError`); uncaught exceptions surface per the F1
exit-code contract. **Why:** idiomatic for the ES6 consumer, uniform with
built-in behavior, and `--script` smoke tests can `try/catch` naturally.
**Alternative rejected:** C-style status codes / null returns — un-idiomatic
and easy to ignore in game scripts.

### D6: Resource model defaults per family
- **JS-managed:** materials (plain objects passed to `efx.setMaterial`),
  mesh/vertex data before upload, colors, transforms — created and dropped
  freely, GC handles lifetime.
- **Slot-based (default for native storage):** textures, meshes, lights —
  fixed-index upload (`efx.setTexture(slot, texData)`, `efx.setMesh(slot,
  data)`), matching vision.md's `setMesh(0, data); useMesh(0)` rule. Slot
  counts appear in a limits table; counts for future milestones are listed
  with provisional values and are pinned by the delivering milestone's
  change.
- **Handle-based:** reserved for resources whose working-set size is genuinely
  dynamic — render targets are the expected F5 case. Handles carry an explicit
  `efx.destroy*(handle)`; the document states leak rules per family.
- Fixed limits table: 4 point lights, 1 directional light, 1 camera (from
  vision.md), plus slot counts per family.

### D7: High-level JS layer ships inside the player
Pure-JS high-level functions (`efx.drawModel`, `efx.drawText`, procedural
primitives like `efx.makeCube`) are engine-provided: bundled with the player
and evaluated into the global scope before `main.js` runs, so user resource
roots never vendor a lib directory and scripts need no imports. The
implementing milestone's change owns the bundling mechanism. **Alternative
rejected:** shipping a JS library file inside each resource root —
boilerplate and version drift.

### D8: Catalog scope = vision.md only, gaps flagged
The catalog contains exactly what vision.md's consumer-API properties imply
(2D quads + blending, cameras, meshes + vertex colors + procedural
primitives, Phong materials + channel maps + alpha masks, lights, render
targets + post FX, resource loading, skinning/animation, `drawModel`/
`drawText`). Vision gaps — input handling and audio are absent from
vision.md — are recorded in the document's "Open questions" section as links
back to vision, **not** invented APIs. A vision→section traceability table
makes the mapping checkable.

### D9: Hook signature `update(dt)` documented as target contract
The document records `update(dt)` / `render()` as the contract, noting that
F1 currently calls hooks with no arguments and that `dt` delivery (seconds
since previous frame) is ratified by the first milestone that needs timed
behavior (F2) — additive and backward-compatible, so no F1 change is implied.

## Risks / Trade-offs

- [Forward-declared provisional signatures drift from what milestones finally
  implement] → Provisional markers are explicit; the spec requires any API
  change to update the document in the same change, so drift is visible in
  every feature PR rather than accumulating silently.
- [Slot counts chosen now may not fit sokol's actual limits] → Counts are
  documented as provisional per family until the delivering milestone pins
  them; only the vision-fixed limits (lights, camera) are presented as final.
- [Single document grows large by F8] → Milestone-section structure keeps
  entries additive; if it ever becomes unwieldy, splitting is a mechanical
  doc refactor that does not change the contract.
- [Degrees-vs-radians or color conventions annoy some consumers] → Conventions
  are stated up front and applied uniformly; changing them later is a
  `js-api` delta like any other API change.

## Migration Plan

Documentation-only rollout: write `docs/js-api.md`, add the `js-api` capability
spec, link the reference from README, and note the doc-sync rule in AGENTS.md.
No code migration. Rollback is reverting the docs/links; the capability spec
is removed by archiving or by a later change if the approach is abandoned.

## Open Questions

- Input and audio: absent from vision.md; flagged in the document's open
  questions rather than speculatively designed. If vision grows these
  capabilities, they enter the catalog through a future change with a
  `js-api` delta.
- Exact slot counts per family (textures, meshes, render targets): provisional
  until F2/F5 pin them.
- REPL-facing API surface (F6): the REPL drives the same `efx` namespace;
  whether it needs extra introspection helpers is deferred to F6.

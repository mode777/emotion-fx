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
  units, error style, resource/memory model, resource classification.
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
- Not a tutorial or engine manual — a reference; each milestone section
  carries exactly one short, complete `main.js` sample (D10) and nothing more.

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

### D6: Resource model — classes for dynamic counts, slots for the light bank
- **JS-managed:** materials (plain objects read by `efx.setMaterial`),
  colors, transforms, and the F8 font object (atlas Texture + quad layout,
  pure JS) — created and dropped freely, GC handles lifetime.
- **Native-backed classes** — the seven-type taxonomy of ADR 0014
  (supersedes 0013): MeshData (skinned meshes carry `joints`/`weights`
  vertex attributes, glTF-style), ImageData, Skeleton (joint hierarchy +
  inverse bind matrices ≈ glTF `skin`), Animation (CPU-side) and Mesh,
  Texture, RenderTarget (GPU). Opaque GC-finalized JS objects wrapping
  native handles — **fully opaque at first (`destroy()` only); query
  methods, getters, and setters are reserved for later** (the class
  machinery supports adding them without changing call sites). Pipeline:
  `createMesh(meshData)` / `createTexture(imageData)` upload CPU → GPU;
  `setSkin(skel, mesh)` is the `node.mesh + node.skin` equivalent, posing
  in place (CPU skinning chosen on engineering grounds — ADR 0014, with
  the constraint clarified in ADR 0015).
  Native byte cost — CPU buffers included — counts toward GC pressure and
  the player collects at frame end, so unreferenced resources are reclaimed
  within roughly a frame even without `destroy()` (ADR 0012). The display
  list pins recorded objects; `destroy()` mid-frame defers the native
  release to frame end.
- **Slot-based:** the light bank only — 4 point slots + 1 directional
  (fixed limits from vision.md). The earlier texture/mesh/skeleton slot
  banks and their slot-count tables are dropped: those counts were
  provisional constants solving an allocation problem that classes remove.
- Fixed limits table: 4 point lights, 1 directional light, 1 camera.
Rationale, consequences, and rejected alternatives: ADR 0011 (model),
ADR 0012 (memory discipline), ADR 0014 (taxonomy + skinning; supersedes
0013).

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

### D9: Hook signatures `init()` / `update(dt)` documented as target contract
The document records `init()` / `update(dt)` / `render()` as the contract.
`init()` runs once after the script is loaded and before the first frame —
setup that needs the engine fully ready, distinct from top-level `main.js`
code which F1 evaluates at load time. F1 currently calls hooks with no
arguments and does not invoke `init()` at all; `dt` (seconds since the
previous frame) and `init()` are ratified by the next runtime change (F2 at
the latest) — both additive and backward-compatible (hook absence is already
tolerated). **Alternative rejected:** treating top-level code as init — it
already has defined load-time semantics in F1 and cannot express "after the
runtime is ready".

### D10: One complete sample per milestone section
Each milestone section (F1–F8) carries one short but complete `main.js`
sample illustrating that milestone's catalog entries: the F1 sample uses
only current behavior, the F2–F8 samples use their provisional APIs plus the
`init()` hook. Samples are illustrative contracts-to-implement, not tested
examples under `examples/`.

### D11: Math representation — plain JS data at the boundary, GLM internal
The F3 math entries (`efx.mat4`/`efx.vec3`/`efx.quat`) are pure JS over
column-major `Float32Array`s, and every math-taking API call accepts plain
arrays, converting in C once per call. GLM (ADR 0005) never reaches scripts.
Full rationale, consequences, and rejected alternatives live in
`docs/decisions/0010-script-math-is-plain-js-data.md`; the `docs/js-api.md`
catalog text is left unchanged here — a follow-up API update will fold the
representation into the entries.

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
- REPL-facing API surface (F6): the REPL drives the same `efx` namespace;
  whether it needs extra introspection helpers is deferred to F6.

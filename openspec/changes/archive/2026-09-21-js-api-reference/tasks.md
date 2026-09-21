# Tasks

## 1. Reference document skeleton

- [x] 1.1 Create `docs/js-api.md` with the section skeleton from design.md: header/overview, Conventions (namespace, naming, option objects, units, errors per D1/D3/D4/D5), Lifecycle hooks, Resource & memory model with fixed-limits table (D6), milestone sections F1→F8, Open questions (D8); verify headings render correctly in a markdown preview
- [x] 1.2 Write the Conventions and Resource & memory model sections fully (rules, limits table: 4 point lights, 1 directional light, 1 camera, provisional slot counts marked provisional); verify every rule stated traces to vision.md or an F1 implementation fact

## 2. API catalog by milestone

- [x] 2.1 Write the F1 section as current behavior: `efx.log(msg)`, `efx.quit(code)`, `efx.args()`, global `update(dt)`/`render()` hooks (noting F1 currently calls hooks without arguments, per design D9); verify each entry matches `src/runtime/runtime.c` registrations exactly
- [x] 2.2 Write provisional catalog entries for F2–F8 (2D quads/blending/camera, meshes/math/primitives, materials/lights/maps/alpha masks, render targets/post FX, resource loading, skinning/animation, high-level `drawModel`/`drawText`) with `provisional` markers, layer tags (`C`/`JS`), and signature sketches per D3; verify every entry names its delivering milestone and layer
- [x] 2.3 Add the vision.md→section traceability table and the "Open questions" entries for input handling and audio (flagged, not invented, per D8); verify each vision.md consumer-API property maps to exactly one catalog section or an open question

## 3. Integration and verification

- [x] 3.1 Add a pointer to `docs/js-api.md` from README.md (Consumer API section) and a one-line doc-sync note in AGENTS.md (API changes require a `js-api` delta + document update in the same change); verify the links resolve
- [x] 3.2 Run `openspec validate --change js-api-reference --strict` and confirm the change passes; re-read `docs/js-api.md` against the four `js-api` spec requirements (namespace rule, layer tags, resource classification + fixed limits, coverage/sync rules) and confirm each is satisfied in the delivered document

## 4. Iteration: init hook + per-milestone samples

- [x] 4.1 Document the `init` lifecycle hook in `docs/js-api.md` as a target contract (F1 invokes only `update`/`render` today); sync the `js-api` spec delta wording, design.md (D9/D10), and the proposal lifecycle bullet
- [x] 4.2 Add one short but complete `main.js` sample to each milestone section F1–F8 (F1 sample uses current API only; F2–F8 samples use their provisional APIs and the `init` hook); extend F7 with `loadSkeleton`/`loadAnimation` and F8 with `loadFont` catalog entries so samples stay grounded
- [x] 4.3 Run `openspec validate --change js-api-reference --strict` and confirm the change passes

## 5. Iteration: F7 skinning clarity

- [x] 5.1 Fix the F7 sample's undefined `POSED` mesh slot: add `efx.setSkin(skelSlot, meshSlot)` (in-place skinning pipeline) to the F7 catalog, document skinned mesh data (`joints`/`weights` attributes on the F3 mesh shape), rewrite the sample around one bound mesh slot, and confirm `openspec validate --strict` passes

## 6. Iteration: math representation decision

- [x] 6.1 Record the math-representation strict split as ADR 0010 (`docs/decisions/0010-script-math-is-plain-js-data.md`) + index row; add design.md D11 referencing it; leave `docs/js-api.md` untouched (API update deferred to a follow-up); confirm `openspec validate --strict` passes

## 7. Iteration: resource model — classes over slots

- [x] 7.1 Write ADR 0011 (`docs/decisions/0011-dynamic-resources-are-gc-finalized-classes.md`: dynamic-count resources as opaque GC-finalized JS classes with idempotent `destroy()`; slots only for the fixed light bank; display-list pinning) and ADR 0012 (`docs/decisions/0012-native-memory-gc-discipline.md`: native bytes counted into GC pressure via allocator hooks, frame-end collection, destroy-first/finalizer-backstop); add both to the decisions index
- [x] 7.2 Sync artifacts: rewrite the `js-api` resource requirement (new classification classes, memory discipline, display-list liveness scenarios), update the proposal's resource-model bullet, rewrite design.md D6 and prune obsolete slot-count material
- [x] 7.3 Rework `docs/js-api.md`: Conventions resource bullet, Resource & memory model section (classes table, lifecycle rules, limits; slot-count table removed), F2–F8 catalog entries and samples on the class-based model (`createTexture`/`createMesh`/class render targets/`createSkeleton`/`createAnimation`/Animation-object `playAnimation`/`blendAnimations`, `loadTexture` as `[JS]` convenience), drop the slot-counts open question; run `openspec validate --strict`

## 8. Iteration: resource taxonomy + glTF skinning model

- [x] 8.1 Pin the resource taxonomy as ADR 0013 (eight GC-finalized opaque types; MeshData/ImageData promoted out of JS-managed plain data; all types fully opaque at first — `destroy()` only, methods/getters/setters reserved; Font demoted to a pure-JS construct over Texture)
- [x] 8.2 Follow the glTF data model per review: ADR 0014 supersedes 0013 — Skin removed as a first-class type, weights/joint-indices live in MeshData vertex attributes, Skeleton (hierarchy + inverse bind matrices) ≈ glTF `skin`, `setSkin(skel, mesh)` ≈ `node.mesh + node.skin`; CPU in-place posing retained (fixed-function constraint); seven types
- [x] 8.3 Sync artifacts and doc: spec taxonomy list, proposal bullet, design.md D6, `docs/js-api.md` (resource table, conventions, F2 `createImageData`/`createTexture(imageData)`, F3 `createMeshData`/`createMesh(meshData)`, F6 `loadMeshData`, F7 glTF bullets + catalog, F8 JS-managed font); mark ADR 0013 superseded in status + index; run `openspec validate --strict`

## 9. Iteration: clarify the fixed-function constraint scope

- [x] 9.1 Write ADR 0015 (`docs/decisions/0015-fixed-function-is-consumer-api-contract.md`): "no programmable shaders" is a consumer-API contract — scripts never see/modify shaders; internals MUST use Sokol's programmable pipeline with engine-owned canned shaders; review test = consumer-API surface, not internal use; + index row
- [x] 9.2 Correct over-broad wording everywhere: vision.md property, AGENTS.md stack + constraint lines, `openspec/config.yaml` context, `docs/js-api.md` traceability row, ADR 0014 (deliberately-not-copied note + GPU-skinning rejection reworded from "impossible by constraint" to engineering economics), design.md D6; run `openspec validate --strict`

## 10. Iteration: lifecycle via explicit hook registration

- [x] 10.1 Write ADR 0016 (`docs/decisions/0016-explicit-hook-registration-implicit-init.md`): `efx.registerUpdateHook`/`efx.registerRenderHook` as the normative lifecycle API (ES6 callbacks, stacking in registration order, unsubscribe returned, `dt` on update hooks); loading `main.js` is the implicit init — runtime guarantees full engine readiness before script evaluation (F1's load-before-window order flips); F1 globals remain load-time sugar so the F1 gate and examples stay valid; REPL rationale + index row
- [x] 10.2 Sync artifacts: spec lifecycle clause (registration model replaces `init`/`update`/`render` globals), proposal lifecycle bullet, design.md D9 rewrite + D10 samples note
- [x] 10.3 Rework `docs/js-api.md`: Lifecycle hooks section (implicit init, registration sketch, sugar rule, REPL note), F1 section pointer, F2–F8 samples to top-level setup + `registerUpdateHook`/`registerRenderHook`, open questions + traceability row; run `openspec validate --strict`

## 11. Iteration: implicit rig payload + `skinned` draw flag

- [x] 11.1 Write ADR 0017 (`docs/decisions/0017-implicit-rig-payload-skinned-flag.md`): Skin/Skeleton/Animation are no longer script resources (5 native types) — rigs and clips bundle into the Mesh at import, playback state inside the Mesh; `skinned` chosen over `drawMeshSkinned` (identical option set + display-list record; per-draw semantics like `color`; three.js/Godot precedent noted); dual bind/posed buffers; `skinned: true` on non-skinned mesh throws; ADR 0014 status amended (glTF data mapping stands, exposure superseded); + index row
- [x] 11.2 Sync artifacts: spec taxonomy list (5 classes + implicit-rig clause), proposal resource bullet, design.md D6 + rationale pointer
- [x] 11.3 Rework `docs/js-api.md`: resource table (5 rows + implicit-rig note on Mesh), F6 `loadMesh` bundling note, F7 section rewrite (`playAnimation`/`pauseAnimation`/`blendAnimations` on the Mesh, `skinned` flag + bullets + sample), F8 `drawModel` forwards `skinned`, open questions (procedural rigs, clip naming); run `openspec validate --strict`

## 12. Iteration: script-driven posing

- [x] 12.1 Write ADR 0018 (`docs/decisions/0018-script-driven-posing.md`): `efx.poseMesh(mesh, pose)` (single sample or weighted array; time wraps, weights normalized, negatives throw) replaces the playback trio — the script owns the clock in the update hook; no engine playback state; stateful playback may return as pure-JS helper (F8 layer); ADR 0017 status amended; + index row
- [x] 12.2 Sync artifacts: spec implicit-payload clause (`posed by the script`), proposal bullet, design.md D6 + rationale pointer
- [x] 12.3 Rework `docs/js-api.md`: F7 section (two-function catalog, posing bullets, cross-fade sample with script-owned clock), Mesh table row (drop pose state), open questions (clip naming via `poseMesh`, stateful-helper candidate); run `openspec validate --strict`

## 13. Iteration: explicit option-object convention

- [x] 13.1 Audit all option-object signatures for true optionality; fix two bag-level mislabels (`createRenderTarget(opts)` and `drawText(text, x, y, opts)` — bags contain required fields); give `setMaterial` channels documented defaults (black/white/black/shininess 32), making them optional per convention
- [x] 13.2 Make the convention explicit: two-level optionality rules (bag-level `?` = fully omittable; field-level `?` = optional only with documented default) + validation rules (missing/wrong-typed required fields and unknown fields throw `TypeError`; nullable "null disables" bags) in `docs/js-api.md` Conventions and design.md D3; run `openspec validate --strict`

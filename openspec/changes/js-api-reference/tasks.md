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

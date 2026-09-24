# Tasks

Implements F3 (3D core). Decisions D1–D12 refer to `design.md`; behavior
requirements live in the delta specs (`specs/3d-core/`, `specs/js-api/`).

## 1. GLM vendoring and math wrapper module (D2)

- [x] 1.1 Vendor a pinned GLM snapshot into `vendor/glm` (used submodules only: core mat4/vec3/vec4 headers), record source repo + pinned commit/hash in `vendor/README.md` per the evaluation in proposal.md; verify a trivial C++ TU including GLM compiles on the host and on an Emscripten configure
- [x] 1.2 Create the `src/math/` module: `efx_math.h` (plain C API, no GLM types) + C++ TUs wrapping the mat4/vec3 operations the engine needs (lookAt, perspectiveY, ortho, mul, translate/rotate/scale compose); wire into CMake (core static lib) with warnings-as-errors behind the ADR 0003/0005 walls; verify with a C unit test composing a known lookAt × perspective and asserting documented matrix values (runs headless in ctest)

## 2. MeshData native class (D8)

- [x] 2.1 Implement the MeshData native-backed class per ADR 0011/0012: engine-owned contiguous per-surface storage copied from JS arrays at creation, GC-pressure accounting for CPU bytes, idempotent `destroy()`, finalizer + teardown finalization; verify with a C unit test that creation copies storage and destroy/finalize releases exactly once
- [x] 2.2 Implement full validation per the multi-surface spec (batch `surfaces` / single-surface shorthand; counts, index ranges, vertex-count divisibility, 16-surface cap → `RangeError`; element types, unknown fields, both/neither form → `TypeError`; plain arrays and typed arrays accepted); verify with a `--script` smoke test per throw case (non-zero exit with diagnostic) plus one valid multi-surface creation (exit 0)
- [x] 2.3 Expose the read-only `surfaceCount` query property (throws `TypeError` when destroyed); verify via smoke script (valid read, post-destroy read throws)

## 3. Mesh upload and lifecycle (D1)

- [x] 3.1 Implement `createMesh(meshData)`: build per-surface interleaved vertex + index buffers (position always; normal/uv/color slots present-or-defaulted per D1) via the platform GPU layer; Mesh is a copy (independent of source lifetime); verify with a C unit test on buffer sizes/counts for a mixed-attribute two-surface fixture
- [x] 3.2 Implement the Mesh native-backed class (destroy/finalizer/teardown, `surfaceCount` query property, use-after-destroy throws); verify via smoke scripts (double destroy is a no-op, destroyed use throws, destroy of source MeshData leaves the Mesh usable)
- [x] 3.3 Reserve the per-surface material binding storage (inert until F4): MeshData creation accepts and stores binding slots; `materials` field rejected as unknown until F4 per spec; verify with a smoke script (materials field → `TypeError`)

## 4. API bindings (record path)

- [x] 4.1 Implement `setCamera3D(opts)` binding: bag validation/defaults (near 0.1, far 100, `TypeError` per spec), separate 3D camera state (D12); verify via smoke scripts (defaults accepted, malformed bags throw, state unchanged after throw)
- [x] 4.2 Implement `drawMesh({ mesh, transform?, color? })` binding: required live Mesh, transform validation (16 finite numbers, column-major; length → `RangeError`, element type → `TypeError`), tint default white, unknown fields → `TypeError`, value-snapshot of camera + options at record time (ADR 0019); verify via smoke scripts per throw case and a C headless test asserting the recorded mesh record (mesh handle, transform floats, tint, camera snapshot — the display-list record/assert gate)
- [x] 4.3 Register all F3 entries on both bindings with identical semantics: `src/api/api.c` (desktop quickjs) and `src/web/bridge.c` (web bridge, ADR 0022); verify `tools/run_web_compare.mjs` shows desktop vs web output agreement for the F3 smoke scripts
- [x] 4.4 Confirm the per-frame record budget treats a multi-surface mesh as one record (playback expands per surface after the check, design Risks); verify with a C unit test recording a 16-surface mesh against the budget

## 5. Display list and playback: depth (D4, D7)

- [x] 5.1 Extend the record struct with the mesh record (handle, `float[16]` transform, tint, camera snapshot) and the 3D pipeline-state flag; keep record order = playback order (F2 stable sort untouched); verify with the existing headless display-list suite extended to mesh records
- [x] 5.2 Add per-pass depth buffer setup (clear to 1.0 beside the color clear), 3D pipeline depth state `less-equal` + write, 2D variants unchanged (`always`/no write); verify with a C headless unit test on pass-state selection per record type, then confirm the committed F2 golden suite still passes byte-identically on the host
- [x] 5.3 Implement playback MVP composition (VP from the recorded camera × record transform per D3, re-composed at playback for the active pass aspect); verify with a C unit test asserting the composed MVP for a known camera/transform pair

## 6. 3D canned pipeline (D5, D6)

- [x] 6.1 Extend the single-source GLSL with the 3D variant (attributes position/normal/uv/color; uniforms MVP + tint; fragment = tint × vertex color, unlit) and regenerate via the pinned sokol-shdc flow (ADR 0021); verify the generated headers build on all four backend paths of a host configure (GL + D3D11 + Metal compile checks via the shdc output targets)
- [x] 6.2 Wire the 3D draw path: per-surface sokol draws in surface order, CCW front / backface cull on (D6); verify manually on the host with a scratch scene (two-surface mesh + tint + vertex colors) before any golden exists

## 7. Procedural primitives and JS math layer (D9, D10)

- [x] 7.1 Implement `makeCube` / `makePlane` / `makeSphere` as bundled pure JS (pinned layouts/formulas per D9, defaults per spec, validation per spec); register on both bindings' prelude; verify via smoke scripts (defaults, overrides, invalid params → non-zero exit) and a script asserting vertex counts and sample positions for each primitive
- [x] 7.2 Implement `efx.mat4` / `efx.vec3` / `efx.quat` as bundled pure JS (pure functions, degrees, column-major, composition per spec); verify via `--script` math unit tests asserting expected matrix values (perspective entries, rotate 90° about Y, multiply order, purity/no-mutation)
- [x] 7.3 Cross-check JS math against the C wrapper (D10): a ctest-driven script compares a fixed input vector of operations between `efx.mat4` results and `src/math` outputs within epsilon; verify it passes headless in ctest on the host

## 8. Golden scenes (D11)

- [x] 8.1 Author the six F3 golden scene resource roots (vertex-colored rotating cube; tinted sphere; two-surface mesh; depth overlap; transform variety; plane grid) at the standard 640×480 using only the settled F3 API; verify each runs in the host player without error
- [x] 8.2 Capture and commit the goldens via the documented regeneration invocation; run the manual server-side capture step on the SSH verification server (llvmpipe-only quirk, `docs/verification-server.md`); verify `ctest` golden tests pass on the host and the F2 golden suite is unchanged

## 9. Docs and ADR

- [ ] 9.1 Write ADR `docs/decisions/0024-multi-surface-meshes.md` per TEMPLATE.md (surface data model, per-surface material bindings, no global material state, efx-verb naming with the method-form alternative, ADR 0014 mapping supersession note) and add it to the `docs/decisions/README.md` index; verify the file exists and is indexed
- [ ] 9.2 Rewrite the `docs/js-api.md` F3 section to the settled contract (multi-surface `createMeshData`, `createMesh`, `drawMesh`, `setCamera3D`, primitives, math layer, limits table row, MeshData/Mesh query properties) and apply the provisional-section ripples: F4 `setMaterial` → `setMeshSurfaceMaterial` (samples updated), F6 primitives→surfaces wording, F7 per-surface joints/weights, F8 `drawModel` redefined; move F3 entries from provisional to current; verify every F3 entry's signature matches the implementation (spot-check each)
- [ ] 9.3 Update AGENTS.md (current-state section and roadmap status table for F3 once verified); verify the table matches reality at the time of the update

## 10. F3 gate (verification order per AGENTS.md)

- [x] 10.1 Host verification: full `ctest` green (smoke + headless display-list/mesh unit tests + math cross-checks + golden suite incl. new F3 scenes) and the F1/F2 suites unchanged; verify via host ctest output
- [x] 10.2 Commit → push branch → `python3 tools/verify_remote.py all <branch>` (Linux golden-bearing jobs as the pre-filter); fix and re-verify until green before any GitHub Actions run (green after the indexed-draw fix: native llvmpipe suite, web Chrome/SwiftShader suite, plus emscripten ctest 40/40 and desktop↔web compare all-match)
- [ ] 10.3 Dispatch `gh workflow run ci.yml --ref <branch>`; the full four-target gate passes in Linux → Windows → macOS order (native suites incl. goldens on each, Emscripten suite + web goldens), per the rendering-milestone gate; verify via the Actions run summary

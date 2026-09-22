# Tasks

Implements F2 (2D layer). Decisions D1–D10 refer to design.md; behavior
requirements live in the delta specs (`2d-layer`, `verification`).

**STATUS: COMPLETE.** Both follow-up changes landed:
`f2a-sokol-shdc` (generated canned shaders; D3D11/Metal quads render,
ADR 0021) and `f2b-web-native-runtime` (quickjs removed from web; the
browser native engine drives the core via the `src/web/` bridge, ADR 0022).
That resolved the D3D11/Metal shader defect and the web `JS_RunGC` crash,
completing the capture/readback and CI-golden tasks. The full four-target
matrix is green (run 35705801110 on HEAD `9746a8d`): native 41/41 on
Windows/Linux/macOS with all six golden scenes, Emscripten 23/23, and the
web golden job all six scenes + harness scenarios.

## 1. Vendoring and render module skeleton

- [x] 1.1 Vendor pinned `stb_image.h` and `stb_image_write.h` snapshots into `vendor/stb/`, record source repo + pinned version/hashes in `vendor/README.md` (evaluation in proposal.md); verify both headers compile in a trivial TU on the host and a round-trip encode/decode of a small buffer matches byte-for-byte
- [x] 1.2 Create the `src/render/` module skeleton (display list, pipeline, playback entry points) behind the ADR 0003 walls, wire it into CMake (core static lib) with warnings-as-errors; verify host configure + build succeeds with no renderer call sites yet

## 2. Display list core (headless, per D1/D3/D4)

- [x] 2.1 Implement the frame-transient record arena and the quad record struct (composed affine, sourceRect, tint, texture handle, blend byte) with record-time compose helpers for the view (center-pivot zoom/rotation, D1) and model (corner-anchored, center-pivot rotation/scale) transforms; verify with a C unit test that a known camera + quad input composes the documented matrix values
- [x] 2.2 Implement the stable-sort infrastructure and sort key field with the F2 key = record index (D3); verify with a C unit test that playback order equals record order and equal keys are stable
- [x] 2.3 Implement the 16 MiB record budget with an overflow error code (D4); verify with a C unit test that exceeding the cap reports budget-exceeded and the arena rewinds cleanly on frame end
- [x] 2.4 Implement playback as a renderer-sink interface emitting batched consecutive same-texture draw ops; verify with a C headless capture-sink unit test asserting op order, batching, and record values (the ADR 0019 record/assert gate; runs in ctest with no GPU)

## 3. API surface (api.c bindings)

- [x] 3.1 Implement `setCamera2D(opts)` binding: frame/x/y/zoom/rotation validation and defaults, record-time snapshot semantics (spec: value-snapshot scenario); verify via `--script` smoke tests (TypeError on bad fields, defaults accepted) and a C test asserting a recorded quad carries the camera at record time
- [x] 3.2 Implement `drawQuad(x, y, w, h, texture, opts?)` binding: required live texture (TypeError on missing/wrong/destroyed), color/rotation/scale/sourceRect validation with bounds-enforced sourceRect (D6); verify via smoke scripts asserting each throw case exits non-zero and one valid call records (checked via C test through the sink)
- [x] 3.3 Implement `createImageData` (RGBA8, length → RangeError) and `createTexture` (sokol image upload, dimensions retained) bindings; verify via smoke script (bad length throws, valid round trip records a texture draw) and a C test that texture bounds feed sourceRect validation
- [x] 3.4 Implement `efx.whiteTexture` (D5): engine-owned 1×1 white Texture, `destroy()` throws TypeError, excluded from finalizer/teardown double-free; verify via smoke script and teardown test
- [x] 3.5 Implement `setBlendMode` (three modes, per-record snapshot) and `setClearColor` bindings; verify via smoke scripts (invalid mode/arity throws, defaults hold)

## 4. 2D canned pipeline (sokol playback)

- [x] 4.1 Implement the engine-owned 2D shader and three blend pipeline variants (D2, ADR 0015), quad vertex generation from records (frame→NDC stretch, y-down flip), and playback into the sokol pass; verify manually on the host with a scratch scene (textured quad + tint + rotation) before any golden exists
- [x] 4.2 Route blend-variant selection and clear color through playback state; verify manually that alpha/additive/subtractive scenes differ visibly and clear color applies

## 5. Capture run mode and readback (D7)

- [x] 5.1 Add `--capture-frame <N> --capture-output <file>` to the player run modes: render N frames, read back, write PNG via stb (with row-order handling), exit 0; verify on the host that a fixture capture produces a 640×480 PNG whose corner pixels match the scene
- [x] 5.2 Implement per-backend readback behind the platform layer: GL `glReadPixels` (Linux/macOS) and D3D11 staging copy + map (Windows); verify GL path on the host and the D3D11 path via the CI Windows job in task 7.1 (verified by f2a task 3.1: Windows D3D11/WARP goldens green)
- [x] 5.3 Implement the Emscripten capture path (WebGL2 readPixels → MEMFS → PNG); verify the build compiles and a Node-driven run produces a capture buffer (verified by f2b task 4.1: web goldens green)

## 6. Golden harness and scenes (D8)

- [x] 6.1 Implement the comparator helper (per-channel Δ ≤ 2/255, ≥ 99.5% pixels, writes `{test}-actual.png` + `{test}-diff.png`, names the test, exits non-zero); verify by forcing a mismatch and confirming artifacts + non-zero exit, then a matching pair exits 0
- [x] 6.2 Author deterministic golden scenes as resource roots (clear color; white-texture solid rect + tint; sourceRect atlas strip; rotation/scale pivot; blending trio; camera zoom/rotation about center) driven by F1 global hooks, and commit their goldens via the documented regeneration invocation; verify `ctest` golden tests pass on the host
- [x] 6.3 Wire golden tests and the display-list unit tests into ctest (all build targets); verify `ctest` on the host runs smoke + unit + golden suites green

## 7. CI determinism (D9)

- [x] 7.1 Update `.github/workflows/ci.yml`: pin emsdk to an exact version; Linux golden job with xvfb + Mesa llvmpipe; Windows job relying on WARP; macOS on the pinned runner image; verify native golden jobs pass (emsdk pinned 3.1.64; Linux xvfb + `LIBGL_ALWAYS_SOFTWARE=1`; Windows WARP; native golden jobs verified by f2a 3.1/3.2. Caveat: macOS runs `macos-latest`, not a pinned runner image)
- [x] 7.2 Add the Emscripten golden job: pinned headless Chrome + SwiftShader flags driving the capture build via a Node script; verify the job passes and its capture matches the committed golden (verified by f2b task 4.1: `golden-web` job green, Chrome 131 + SwiftShader)
- [x] 7.3 Document the golden regeneration procedure (invocation, when to regenerate) in the README; verify the documented command reproduces a committed golden byte-comparable under tolerance on the host

## 8. Docs and ADR

- [x] 8.1 Write ADR `docs/decisions/0020-golden-image-verification.md` (tolerance policy, capture strategy, software-rasterizer + pinning rationale) per TEMPLATE.md and add it to the `docs/decisions/README.md` index; verify the file exists and is indexed
- [x] 8.2 Rewrite the `docs/js-api.md` F2 section to the settled contract (signatures, defaults, budget, white texture, sourceRect semantics), refresh the F5 provisional sample to the new `drawQuad` shape, and move F2 entries from provisional to current; verify every F2 entry's signature matches the implementation (spot-check each)
- [x] 8.3 Update AGENTS.md (current-state section and roadmap status table for F2); verify the table matches reality at the time of the update

## 9. F2 gate (broader verification)

- [x] 9.1 Full CI run: build matrix + smoke suite + display-list unit tests + golden tests green on Windows, Linux, macOS, Emscripten; verify via the Actions run summary per the verification spec's rendering-milestone gate (run 35705801110 on HEAD `9746a8d`: native 41/41 each, Emscripten 23/23, web goldens all six scenes pass)
- [x] 9.2 Confirm the F1 suite still passes unchanged (no regression) and the F1 windowed checklist still holds on the host; verify `ctest` smoke results in the same run (confirmed by f2b: desktop suites unaffected and green)

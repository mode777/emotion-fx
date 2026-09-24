# Tasks

## 1. Renderer: generalized pivot

- [x] 1.1 Generalize `efx_quad_matrix` in `src/render/render.c` to take a pivot
  `(ox, oy)` (quad-local, anchor `x + ox, y + oy`), add the origin parameter to
  `efx_render_quad` (`src/render/render.h`), and update every call site to pass
  the old default (`size/2` per axis) so behavior is bit-identical: `api.c`,
  `src/web/bridge.c` `_efx_bridge_draw_quad` (+ its `entry.js` call passes the
  center for now). Verify: clean build on Linux + Emscripten and the existing
  ctest smoke + unit suites pass unchanged (no JS-visible change yet).

## 2. Desktop binding (quickjs)

- [x] 2.1 Reshape `efx_js_drawQuad` (`src/api/api.c`) to
  `drawQuad(x, y, texture, opts?)`: live-texture check first, then size
  derivation `opts.size` → `opts.sourceRect` `w`/`h` → `efx_render_texture_size`,
  `size`/`origin` parsing and validation per the delta spec (TypeError for
  wrong types/non-finite, RangeError for ≤ 0 or zero-extent sourceRect),
  default origin = determined size's center, pass the resolved pivot to
  `efx_render_quad`; move the `JS_CFUNC_DEF` length hint in
  `src/runtime/runtime.c` from 6 to 4. Verify: headless
  `efx --script` run of a scratch script covering `drawQuad(x, y, tex)`,
  `drawQuad(x, y, tex, {size})`, `drawQuad(x, y, tex, {sourceRect})` exits 0
  and each invalid form throws with the spec'd error type.
- [x] 2.2 Add read-only `width`/`height` getter properties to the Texture class
  in `api.c` (`JS_CGETSET_DEF` reading through `efx_render_texture_size`;
  `TypeError` when the texture is destroyed), covering textures created via
  `createTexture` and `efx.whiteTexture` alike. Verify: headless script reads
  `width`/`height` on both, and reading after `destroy()` throws `TypeError`
  (non-zero exit).

## 3. Web bridge parity (ADR 0022)

- [x] 3.1 Reshape `drawQuad` in `src/web/entry.js` to the identical contract
  (ES5 style: derivation order, `size`/`origin` validation, zero-extent
  `RangeError`, default center pivot) and extend `src/web/bridge.c` to forward
  the origin through `_efx_bridge_draw_quad`. Verify: `tools/run_web_compare.mjs`
  reports desktop vs web parity for the quad paths; Emscripten build clean.
- [x] 3.2 Add `width`/`height` properties to the web `EfxTexture` class backed
  by the existing `_efx_bridge_texture_width/height` entry points, throwing
  `TypeError` on a destroyed texture, mirroring 2.2 exactly. Verify: the same
  getter smoke script runs identically on the web runtime (web compare /
  headless Chrome harness).

## 4. Tests and golden scenes

- [x] 4.1 Update `tests/unit/api_tests.c`: rewrite existing `drawQuad` evals to
  the new signature; add sink-asserted cases — 1:1 derivation (recorded quad
  `w`/`h` equal texture pixels), sourceRect-derived size, `size` override with
  `scale` applied after, origin pivot geometry (e.g. `origin: [0,0]` + rotation
  90 keeps the top-left corner fixed), the throw matrix (`size: [0,10]`,
  `size: [10]`, `size: 'big'`, `origin: [NaN,0]`, zero-extent sourceRect), and
  texture getter values + destroyed-throw. Verify: ctest unit suite green.
- [x] 4.2 Update the portable smoke scripts (`tests/scripts/s_2d_validation.js`,
  `s_resource_lifecycle.js`, …) to the new signature and add new-throw and
  getter cases, staying free of Node/browser dependencies. Verify: ctest smoke
  suite green on desktop and through the web bridge.
- [x] 4.3 Rewrite the six golden scenes (`tests/goldens/*/main.js`) to the new
  signature — drop `w`/`h` where it equals the derived size, pass explicit
  `size` where scenes stretch or draw `whiteTexture` (derives to 1×1) — and do
  NOT touch the committed PNGs. Verify: golden-image harness (ctest with
  `-DEFX_BUILD_GOLDEN_TESTS=ON` under `xvfb-run`) passes with zero PNG changes.
- [x] 4.4 Add a new `origin` golden scene covering 1:1 derived draw,
  sourceRect-derived size, `size` + `scale` ordering, and an off-center pivot
  under rotation; register it in the golden test list and capture its expected
  PNG via `--capture-frame` on the canonical rasterizer (llvmpipe; capture on
  the verification server per `docs/verification-server.md`). Verify: the new
  scene passes locally under `xvfb-run` and on the server.

## 5. Call sites and docs

- [x] 5.1 Rewrite `examples/browser/main.js` to the new signature. Verify: the
  Emscripten web bundle builds and the demo renders correctly (headless-Chrome
  spot check or manual load of the built page).
- [x] 5.2 Audit the repo for stale call sites: `grep -rn "drawQuad("` across
  `src/`, `tests/`, `examples/`, `docs/`, `README.md` (archived change records
  under `openspec/changes/archive/` are historical and stay untouched). Verify:
  no live call site uses the old 6-argument positional form.
- [x] 5.3 Update `docs/js-api.md`: rewrite the F2 `drawQuad` entry (signature,
  derivation order, `size`, `origin`, zero-extent rule, error types) and the
  Texture resource entry (`width`/`height` getters), refresh the F2 sample and
  the F5 provisional sample to the new shape; update the README `drawQuad`
  mention. AGENTS.md needs no edit (it names `drawQuad` without a signature).
  No ADR — per proposal. Verify: spot-check every documented signature/error
  against the implementation (desktop and bridge).

## 6. Verification gate (F2 follow-up: full four-target matrix, ADR 0020/0023)

- [x] 6.1 Local Linux pipeline: configure with `-DEFX_BUILD_GOLDEN_TESTS=ON`,
  build, run the full ctest suite (smoke, unit, all golden scenes incl.
  `origin`), then the Emscripten build + web golden suite in headless Chrome.
  Verify: all green locally before any push.
- [x] 6.2 Commit to a branch, push, and run the SSH pre-filter:
  `python3 tools/verify_remote.py all <branch>` (credentials from
  `SSH_HOST`/`SSH_USER`/`SSH_PASSWORD` env only). Fix and re-verify until
  green; do not dispatch GitHub Actions before this passes.
- [x] 6.3 Dispatch the gate: `gh workflow run ci.yml --ref <branch>`, then
  triage failures strictly Linux → Windows → macOS until the four-target
  matrix is green (manual-dispatch run, no Pages deployment involved).

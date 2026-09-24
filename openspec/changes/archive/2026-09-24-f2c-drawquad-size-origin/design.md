# Design

## Context

F2's `drawQuad` is bound twice with identical semantics: quickjs
(`src/api/api.c`, registered in `src/runtime/runtime.c` as arity-hint 6) and
the web bridge (`src/web/entry.js` + `src/web/bridge.c`, ADR 0022). The call
records into the display list via `efx_render_quad` (`src/render/render.c`),
which immediately bakes position/rotation/scale into an affine
(`efx_quad_matrix`, corner-anchored placement, center pivot); the record
stores only the matrix, size, texture, source rect, tint, blend. Texture
pixel size is available at record time through `efx_render_texture_size`
(desktop) and `_efx_bridge_texture_width/height` (web) — it already backs the
`sourceRect` bounds check. The Texture JS class is fully opaque: no
properties, only `destroy()`. Six golden scenes, the browser example, smoke
scripts, and `tests/unit/api_tests.c` pin today's signature and pixels.
Motivation and scope: see proposal.md; normative behavior: the spec deltas.

## Goals / Non-Goals

**Goals:**

- One positional shape, `drawQuad(x, y, texture, opts?)`, with size derived
  `size` → `sourceRect` → texture pixels, resolved entirely at record time.
- `origin` as a generalized pivot that is a strict superset of today's center
  pivot: same float pipeline, so rewritten scenes reproduce committed goldens.
- Texture `width`/`height` read-only getters with identical semantics on both
  runtimes.
- Pixel-equivalence proof: every existing golden scene keeps its committed
  PNG; only scripts change.

**Non-Goals:**

- Per the proposal: no non-uniform scale, no named/normalized origins, no
  ImageData getters, no display-list/playback changes, no F3+ surface.
- No deprecation shim for the old signature (pre-1.0 engine, no shipped
  external scripts; all call sites are in-repo and rewritten here).
- No renderer record-layout change: records still store a baked affine.

## Decisions

**D1 — Breaking reshape, no overload (user decision).**
`drawQuad(x, y, texture, opts?)` is the only form; `w`/`h` move behind
`opts.size`.
- *Rejected: type-discriminated overload* (accept both `…w, h, texture…` and
  `…texture…`, pick by argv type) — lost: two positional grammars in every
  binding, doc, and test, permanent compatibility debt, and the common case
  stays noisy for anyone copying old code.
- *Rejected: nullable slots* (`drawQuad(x, y, null, null, tex)`) — lost:
  worst call syntax of the three, defeats the whole point.

**D2 — Size derivation and `size` shape resolved in the API layer.**
`size` is a `[width, height]` array (matches the `setCamera2D`
`frame: [w, h]` convention; *rejected: object* `{w, h}` — a second object
form next to `sourceRect` adds verbosity, and arrays are the established
size-pair shape). Derivation order `size` → `sourceRect.w/h` → texture
pixels; `scale` applies afterwards (pre-scale size). The lookup uses the
existing `efx_render_texture_size` / bridge equivalents — no render-layer
change. Zero-extent tightening: `sourceRect` with `w` or `h` == 0 now throws
`RangeError` (previously recorded a degenerate invisible quad); no shipped
scene uses one, and derivation would otherwise turn a typo into a silent
no-op draw.

**D3 — Origin is a pivot in quad-local pixels; default center (user
decision).**
`efx_quad_matrix` generalizes its pivot from `(w/2, h/2)` to `(ox, oy)` with
anchor `x + ox, y + oy`: `tx = x + ox − (a·ox + c·oy)`,
`ty = y + oy − (b·ox + d·oy)`. With the default
`ox = w·0.5f, oy = h·0.5f` this is float-for-float today's matrix (same op
order: `px = x + pivot`, `plx = pivot`), which is what lets the six rewritten
scenes hit their committed PNGs. `efx_render_quad` grows an origin
parameter; the API layer resolves the default center from the determined
size and always passes an explicit pivot. Unrotated/unscaled placement stays
top-left at `(x, y)` for any origin — the offset is never rotated or scaled.
- *Rejected: placement offset* (shift quad, keep center pivot) — lost:
  exactly equivalent to adding origin to `x`/`y`; zero new capability.
- *Rejected: raylib-style anchor* (origin point lands at `x`,`y` and is the
  pivot) — lost: cannot keep both placement and pivot compatibility, so
  every existing scene's pixels change; also changes what untransformed
  `drawQuad` calls mean.

**D4 — Texture getters read through to the render layer; no cached copy.**
`width`/`height` are quickjs `JS_CGETSET_DEF` accessors on the existing
texture class that call `efx_render_texture_size(handle)` at read time and
throw `TypeError` first when the texture is destroyed (same
destroyed-resource rule as draw calls). The web `EfxTexture` class gains
`width`/`height` properties backed by the existing
`_efx_bridge_texture_width/height` entry points with the same throw.
- *Rejected: caching `w`/`h` in the opaque struct at creation* — lost:
  duplicates render-layer state, needs a special path for the internally
  created `whiteTexture`, and can drift (F6 re-upload/resize); the render
  layer is the single source of truth and reads are rare.

**D5 — Dual-runtime parity in the same task series (ADR 0022 pattern).**
Every semantic change lands in `api.c` and `entry.js` together: option
parsing, validation order, error types and messages, derivation, pivot
default. Portability smoke scripts and `tools/run_web_compare.mjs` keep the
runtimes honest. Bridge JS stays in the file's ES5 style.

**D6 — Golden strategy: rewrite scenes, never re-baseline pixels.**
The six existing scenes are rewritten to the new signature — dropping
`w`/`h` where it equalled the derived size (full-texture `srcrect` call),
passing explicit `size` where they stretched (the other `srcrect` calls) or
where derivation is degenerate (`pivot`, `solid`, `blend`, `camera` use
`whiteTexture`, which derives to 1×1). Committed PNGs are untouched: a green
golden suite after the rewrite is the proof the reshape is
pixel-preserving. New behavior gets one new scene (`origin`, llvmpipe-
captured via `--capture-frame` per ADR 0020) covering 1:1 derived draw,
sourceRect-derived size, `size` + `scale` ordering, and an off-center pivot.

**Resource exposure (standing rule).** Unchanged: Texture remains a
GC-finalized opaque class with explicit `destroy()` (ADR 0011/0012);
`width`/`height` are read-only queries on it, not new resources. No new
resource types; lights stay a fixed bank.

## Risks / Trade-offs

- [Breaking change ripples wider than the obvious files] → task includes a
  repo-wide `drawQuad(` audit (live code and docs only — archived change
  records under `openspec/changes/archive/` are historical and untouched).
- [Pivot refactor silently changes pixels for existing scenes] → D6: PNGs
  never re-captured; the golden suite fails if any pixel drifts beyond the
  ADR 0020 tolerance (expected: zero drift).
- [QuickJS getter and bridge property drift apart over time] → smoke script
  asserts identical values and destroyed-throw on both runtimes;
  `run_web_compare.mjs` covers the quad paths.
- [`size` + huge derived textures make accidental 1:1 fullscreen draws] →
  accepted; sizes are script-chosen frame pixels, same exposure as today's
  explicit `w`/`h`.
- [Old arity hint kept by mistake] → runtime.c hint moves 6 → 4 in the
  binding task (cosmetic but keeps QuickJS argv handling honest).

## Migration Plan

All in-repo call sites migrate in this change (bindings → renderer → both
runtimes' tests → goldens → examples → docs). No data format, no network,
no persistence; rollback is reverting the change. Verification follows the
repo gate order: local build + ctest, `tools/verify_remote.py all <branch>`
(SSH server: native goldens under Xvfb/llvmpipe + Emscripten golden suite),
then dispatch the four-target `ci.yml` on the branch (Linux → Windows →
macOS triage order, ADR 0020/0023).

## Open Questions

None — the material choices (signature shape, origin semantics, getter
scope) were settled with the user before proposal; the rest is pinned in the
spec deltas.

## Apply notes — infrastructure quirks

Glitches and quirks hit while implementing (kept in the change record; the
durable ones are mirrored in `docs/verification-server.md`):

- **openspec CLI ignores `rules.design`** — every `openspec` invocation
  printed `Rules for 'design' must be an array of strings, ignoring this
  artifact's rules`, although `openspec/config.yaml`'s `rules.design` is a
  well-formed array of strings. CLI-side parsing issue (package version
  pinned via package.json); the design rules were still honored because the
  config was read directly during planning. Cosmetic but noisy; revisit on
  CLI upgrade.
- **No capture mode in the verification tooling** — adding a golden scene
  needs `golden.png` committed before `verify_remote.py web` can pass (the
  web driver enumerates every scene dir), but captures are only possible on
  the server (llvmpipe). `verify_remote.py` has no `capture` suite, so this
  change used a one-off SSH script: sync branch → build player →
  `xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 player --capture-frame 2` twice →
  `efx_imgdiff` determinism check → SFTP the PNG back. Now documented as the
  standard flow in `docs/verification-server.md`.
- **tasks.md checkbox glitch** — the edit marking task 5.3 done reported
  success but the committed file still contained `- [ ]` (caught by a
  post-commit grep; fixed in 3459cb6). One-off tooling glitch, no content
  impact.
- **`/tmp/opencode` not writable** — the tool description claims it is
  pre-approved for scratch work, but it is root-owned in this container
  (the workspace-global AGENTS.md already warns this). Plain `/tmp` works.
- **Local build cache footgun** — the local `build/` was configured with
  `-DEFX_BUILD_GOLDEN_TESTS=ON`, so bare `ctest` fails in this
  display-less container; use `ctest -E golden` locally and let the
  server/gate run goldens (matches `docs/verification-server.md`).
- **GitHub runner watch items (observed in gate run 35989619506 logs)** —
  actions are forced from deprecated Node 20 onto Node 24 (cosmetic), and
  GitHub announced `ubuntu-latest` migrates to Ubuntu 26 from 2026-10-19.
  The migration may bump Mesa/llvmpipe in the canonical golden job; if
  golden drift appears after that date, the tolerance/re-baseline rules of
  ADR 0020 apply.

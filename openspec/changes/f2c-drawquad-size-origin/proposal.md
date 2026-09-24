# Proposal

## Why

`drawQuad` forces every caller to spell out `w`/`h` even in the overwhelmingly
common case — draw a texture (or a sub-rectangle of it) at its natural pixel
size. The engine already knows a texture's pixel size, so the redundant
arguments are noise, invite aspect-ratio mistakes, and make sprite code longer
than it needs to be. Related gaps in the same surface: scripts cannot rotate or
scale a quad around anything but its center (no origin/pivot control), and the
Texture class is fully opaque — scripts cannot even ask for the size the engine
is about to use for derivation. Fixing all three together keeps one coherent
story: size is derived from the texture unless overridden, and transforms pivot
where the script says.

## What Changes

- **BREAKING** — `drawQuad` is reshaped from `drawQuad(x, y, w, h, texture, opts?)`
  to `drawQuad(x, y, texture, opts?)`. Positional `w`/`h` are removed.
  Every existing call site (examples, golden scenes, smoke and unit tests,
  docs samples) is rewritten; expected golden pixels do not change.
- Size determination order: the new `opts.size` (`[width, height]` in frame
  pixels) if given → else `opts.sourceRect` `w`/`h` → else the texture's pixel
  size (`whiteTexture` is 1×1). `opts.scale` applies after the size is
  determined: `size` is pre-scale frame pixels, scale still multiplies the
  drawn extents.
- New `opts.origin` (`[px, py]`, quad-local frame pixels relative to the
  quad's top-left): the pivot point that `rotation` and `scale` act around.
  Default is the determined size's center, so every rewritten call renders
  pixel-identically to today. The origin offset itself is never rotated or
  scaled; unrotated/unscaled quads still place their top-left at `(x, y)`.
- **BREAKING (tightening)** — a determined size of ≤ 0 in either dimension is
  a `RangeError`; a zero-extent `sourceRect` therefore throws instead of
  recording an invisible degenerate quad (no shipped scene uses one).
- The Texture class gains read-only `width` and `height` getter properties
  (texture pixels). Using them on a destroyed texture throws `TypeError`,
  consistent with the "destroyed resources are unusable" rule. This is the
  first exercised instance of the js-api spec's reserved-for-later getter
  allowance; ImageData stays fully opaque.
- Verification: display-list/unit tests cover derivation order, `size`/`origin`
  validation, pivot geometry; smoke scripts cover both runtimes; new golden
  scene(s) pin origin/derived-size pixels; existing six golden scenes are
  rewritten to the new signature and must reproduce their committed PNGs
  (proves the reshape is pixel-preserving).

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `2d-layer`: the Quad drawing requirement is rewritten — new
  `drawQuad(x, y, texture, opts?)` signature, size-derivation order
  (`size` → `sourceRect` → texture), `size`/`origin` options, pivot-point
  origin semantics with center default, zero-extent tightening. The Image and
  texture resources requirement gains the Texture `width`/`height` getters and
  their destroyed-resource behavior.
- `js-api`: the Resource classification requirement's "fully opaque at first;
  query methods, getters, and setters are reserved for later" clause is
  updated — native-backed classes MAY expose documented read-only query
  getters, and Texture does (`width`, `height`).

## Impact

- `src/api/api.c` / `src/api/api.h` — `efx_js_drawQuad` reshaped (arity 4,
  argument discrimination gone with it, `size`/`origin` option parsing and
  validation, size derivation via `efx_render_texture_size`); Texture class
  gains getter properties.
- `src/runtime/runtime.c` — `JS_CFUNC_DEF` length hint for `drawQuad` (6 → 4).
- `src/render/render.c` / `render.h` — `efx_render_quad` / `efx_quad_matrix`
  gain the pivot (origin) so records bake the same affine with a generalized
  pivot point; `efx_render_texture_size` already provides derivation input.
- `src/web/bridge.c` / `src/web/entry.js` — bridge `drawQuad` mirrors the new
  binding semantics exactly (ADR 0022 parity); `EfxTexture` gains
  `width`/`height` properties backed by the existing bridge texture-size
  entry points.
- Call-site rewrites: `examples/browser/main.js`, `tests/goldens/*/main.js`
  (six scenes, committed PNGs unchanged), `tests/scripts/*.js`,
  `tests/unit/api_tests.c`.
- Docs: `docs/js-api.md` (F2 section rewritten to the new contract — required
  by AGENTS.md for any js-api delta), README `drawQuad` mention. AGENTS.md
  needs no change (it names `drawQuad` without a signature).
- **No ADR** — no durable architecture decision is settled: the opaque-class
  pattern (ADR 0011/0013) already anticipated getters ("reserved for later")
  and this change exercises it; the signature break follows the F2 precedent
  of documenting breaks in the change record. The pivot/derivation semantics
  are pinned by the `2d-layer` spec, which is the right home for behavior.

## Non-goals

- No non-uniform `scale` (`[sx, sy]`) — still a single uniform factor.
- No normalized or named origin shortcuts (`'center'`, `'topleft'`,
  `[0..1]` fractions) — plain pixel offsets only.
- No `width`/`height` (or other) getters on ImageData; Texture only.
- No changes to `drawQuad` blend/camera/display-list semantics, the record
  budget, or renderer playback order.
- No new resource types, no shader-visible anything (fixed-function contract,
  ADR 0015).
- F3+ stays untouched; this is F2 follow-up work (same pattern as f2a/f2b).

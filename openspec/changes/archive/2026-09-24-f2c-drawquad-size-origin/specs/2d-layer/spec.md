# Spec Delta

## MODIFIED Requirements

### Requirement: Quad drawing
`drawQuad(x, y, texture, opts?)` SHALL record one textured quad. `x` and `y`
SHALL place the quad's top-left corner in frame pixels. The `texture`
argument SHALL be required and MUST be a live Texture — passing nothing, a
non-texture value, or a destroyed texture SHALL throw `TypeError`.

The quad's size SHALL be determined in frame pixels by the first match of:
`opts.size` — a `[width, height]` array of finite numbers, both required and
> 0; else the drawn texture region's extent when `opts.sourceRect` is given;
else the texture's pixel size. A `size` entry that is non-numeric, infinite,
or ≤ 0 SHALL throw (`TypeError` / `RangeError` respectively) and record
nothing. A `sourceRect` of zero extent in either dimension SHALL throw
`RangeError` and record nothing. `opts.scale` SHALL apply after the size is
determined: size is pre-scale frame pixels and scale multiplies the drawn
extents.

Options: `color` (tint `[r, g, b, a]`, default opaque white), `rotation`
(degrees clockwise, default 0), `scale` (uniform positive factor, default 1;
non-finite SHALL throw `TypeError`, ≤ 0 SHALL throw `RangeError`),
`sourceRect` (the texture region drawn, `{ x, y, w, h }` in texture pixels,
default the full texture; a region extending outside the texture bounds SHALL
throw `RangeError`), and `origin` (a `[px, py]` array of finite numbers — the
pivot point for rotation and scale, expressed in quad-local frame pixels
relative to the quad's top-left; default the determined size's center). The
origin offset SHALL NOT itself be rotated or scaled, and an unrotated,
unscaled quad SHALL place its top-left corner at `(x, y)` regardless of
`origin`. Unknown or wrongly-typed option fields SHALL throw `TypeError`.

#### Scenario: Textured quad with defaults
- **WHEN** a script calls `drawQuad(0, 0, tex)` for a 128×128 texture with
  known pixel colors
- **THEN** the full texture is drawn 1:1, tinted white, into the 128×128
  frame area at the requested position

#### Scenario: Size derives from the source rect
- **WHEN** `drawQuad(40, 40, tex, { sourceRect: { x: 0, y: 0, w: 64, h: 32 } })`
  is called on a 128×128 texture
- **THEN** the named region is drawn 1:1 into a 64×32 frame area at the
  requested position

#### Scenario: Explicit size overrides derivation
- **WHEN** `opts.size` names a different area than the source region or
  texture size, e.g. a 64×32 source region drawn with `size: [128, 128]`
- **THEN** the region is stretched to fill the named size, overriding the
  source-rect and texture-size derivation

#### Scenario: Scale applies after size determination
- **WHEN** a 64×64 texture is drawn with `size: [32, 16]` and `scale: 2`
- **THEN** the drawn quad covers a 64×32 frame area, pivoting on its center

#### Scenario: Source rect selects a texture region
- **WHEN** `sourceRect` names the top-left quarter of the texture
- **THEN** only that region is sampled and drawn, stretched to the
  destination size

#### Scenario: Out-of-bounds source rect throws
- **WHEN** `sourceRect` extends beyond the texture's pixel dimensions
- **THEN** the call throws `RangeError` and records nothing

#### Scenario: Zero-extent source rect throws
- **WHEN** `sourceRect` has `w` or `h` equal to 0
- **THEN** the call throws `RangeError` and records nothing

#### Scenario: Rotation and scale pivot on the quad center
- **WHEN** a quad is drawn with rotation 90 and scale 2 and no `origin`
- **THEN** the quad's center stays at the center of the determined size's
  rectangle at `x, y`, and the quad rotates clockwise and doubles in size
  around it

#### Scenario: Origin moves the pivot
- **WHEN** a quad is drawn with `origin: [0, 0]` and rotation 90
- **THEN** the quad rotates clockwise around its top-left corner, which stays
  fixed at `(x, y)`

#### Scenario: Origin does not move an untransformed quad
- **WHEN** two identical quads are drawn without rotation or scale, one with
  and one without `origin`
- **THEN** both render pixel-identically with their top-left corner at
  `(x, y)`

#### Scenario: Invalid size or origin values throw
- **WHEN** `drawQuad` is called with `size: [0, 10]`, `size: [10]`,
  `size: 'big'`, or `origin: [NaN, 0]`
- **THEN** the call throws (`RangeError` for out-of-range numbers,
  `TypeError` for wrong types) and records nothing

#### Scenario: Missing or destroyed texture throws
- **WHEN** `drawQuad` is called with no texture, a non-texture value, or a
  texture on which `destroy()` was already called
- **THEN** the call throws `TypeError` and records nothing

### Requirement: Image and texture resources
`createImageData({ width, height, pixels, format? })` SHALL build CPU-side
pixel data: `pixels` is a flat byte array in RGBA8 order of length exactly
`width × height × 4` (wrong length SHALL throw `RangeError`), and `format`
defaults to `'rgba8'` (the only format in F2). `createTexture(imageData)`
SHALL upload image data to a GPU Texture — an opaque native-backed class
released by `destroy()` with GC finalizer backstop (ADR 0011/0013); the
ImageData remains valid afterwards. A live Texture SHALL expose read-only
`width` and `height` properties naming its pixel size; reading either on a
destroyed texture SHALL throw `TypeError`. The engine SHALL expose
`efx.whiteTexture`, an engine-owned 1×1 opaque-white Texture usable in any
draw: scripts SHALL NOT destroy it — `destroy()` on it SHALL throw
`TypeError` — and it SHALL remain valid for the whole run.

#### Scenario: Image to texture round trip
- **WHEN** a script builds an ImageData of known colors, creates a texture,
  and draws it full-frame
- **THEN** the rendered frame shows exactly those pixel colors

#### Scenario: Texture reports its pixel size
- **WHEN** a script reads `width` and `height` on a Texture created from a
  64×32 ImageData, and on `efx.whiteTexture`
- **THEN** the values are 64 and 32, and 1 and 1 respectively

#### Scenario: Destroyed texture getters throw
- **WHEN** a script reads `width` or `height` on a Texture after calling
  `destroy()` on it
- **THEN** reading throws `TypeError`

#### Scenario: White texture draws solid rects
- **WHEN** a script draws `efx.whiteTexture` with `color: [1, 0, 0, 1]`
- **THEN** a solid red rectangle appears, and calling
  `efx.whiteTexture.destroy()` throws `TypeError`

#### Scenario: Pixel buffer length is validated
- **WHEN** `createImageData` receives a `pixels` array whose length does not
  match `width × height × 4`
- **THEN** the call throws `RangeError`

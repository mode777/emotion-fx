# Proposal

## Why

The project is pre-implementation: `vision.md` lists ~30 desired properties but no
agreed order to build them. Without a milestone ladder, individual OpenSpec changes
would be proposed ad-hoc, risk hiding in the wrong places (4-platform build matrix,
the JS/C boundary, the fixed-function shader emulation), and each change would need
to invent its own verification approach. A fixed roadmap turns the vision into a
stacked sequence of independently verifiable milestones and gives every future
proposal a defined place in the sequence.

## What Changes

- Establishes an ordered feature roadmap (F1–F8) that decomposes `vision.md` into
  milestone changes, each stacking on the previous one:
  - **F1 Player skeleton** — CMake + vendored Sokol/QuickJS, window, resource root,
    `main.js` lifecycle hooks, `--script` headless-ish run mode; builds on all four
    targets (Win, Linux, macOS, Emscripten) from day one.
  - **F2 2D layer** — `drawQuad`, ortho camera, texture slots, blending modes,
    display list (record → playback), golden-image verification harness.
  - **F3 3D core** — camera, mesh slots, `drawMesh`, matrix math, depth, vertex
    colors, procedural primitives.
  - **F4 Lighting + Phong** (split F4a/F4b) — directional + 4 point lights,
    4-channel Phong, per-channel maps, alpha masks; settles the canned-shader
    strategy (mega-shader vs build-time permutations).
  - **F5 Render targets + post FX** — RTT, fullscreen-quad passes, color filter,
    blur.
  - **F6 Resource packaging** — zip resource root, real asset import (mesh/texture
    formats), interactive REPL mode.
  - **F7 Skinning + animation** — CPU skinning, skeleton/animation import,
    play/pause/blend.
  - **F8 High-level JS layer + text** — `drawModel`, `drawText` (font atlas on top
    of quads), demo resource pack.
- Defines per-milestone verification strategy: build matrix + script smoke tests
  (F1), golden-image pixel-diff harness (F2 onward), CPU reference
  implementations for lighting/skinning math (F4, F7).
- Documents the roadmap in `AGENTS.md` so future sessions know where a change
  belongs in the sequence.
- Records open cross-cutting decisions (canned-shader strategy, math library,
  QuickJS flavor, asset format, CI determinism) and assigns each to the milestone
  where it must be settled.

## Capabilities

### New Capabilities
- `feature-roadmap`: The project's ordered milestone ladder (F1–F8) — ordering
  constraints between milestones, the verification strategy each milestone must
  satisfy before the next starts, and the rule that proposals must name their
  roadmap position.

### Modified Capabilities
- (none — no specs exist yet)

## Impact

- `AGENTS.md` gains a roadmap section (user-requested).
- No source code exists yet; this change affects no code. It constrains all future
  feature proposals (F1–F8), which will each introduce their own capabilities.
- Future dependency chain: every feature change must declare which milestone it
  implements and must not start before its predecessor's verification passes.

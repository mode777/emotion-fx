# AGENTS.md

Preliminary guidance — the project is **pre-implementation**. The only source of
truth is `vision.md`; read it before proposing anything. Do not assume build
commands, directory layouts, or toolchains that don't exist yet.

## Current state

- No source code, no CMake file, no tests yet. `package.json` exists only to
  install the OpenSpec CLI (`npm install`, then `npx openspec`).
- Specs live under `openspec/`. Work is driven through the OpenSpec SDD flow —
  use the `opsx-*` / `openspec-*` commands and skills (propose → apply →
  archive) rather than coding ad-hoc.

## Planned stack (from vision.md — not yet built)

- C/C++ core, rendering via **Sokol**; **QuickJS** embedded as the ES6 runtime
  on native platforms; Emscripten bridge for the browser (no Node).
- Build system will be CMake; targets: Win, Linux, macOS, Emscripten; output is
  a single binary "player" for a resource folder/zip with a `main.js` entry
  (godot `res://`-style resource root).

## Roadmap

`vision.md` decomposes into a fixed ladder of milestones — the normative spec is
`openspec/specs/feature-roadmap`. The order is fixed: a milestone must not start
before its predecessor's verification gate passes on all four targets, and every
feature proposal must name the milestone it implements.

| # | Milestone | Scope (one line) | Verification gate | Status |
|---|-----------|------------------|-------------------|--------|
| F1 | Player skeleton | CMake + vendored Sokol/QuickJS, window, resource root, `main.js` hooks, `--script` run mode | Builds on Win/Linux/macOS/Emscripten; script smoke test crosses the JS/C boundary and exits 0 on each | done |
| F2 | 2D layer | `drawQuad`, ortho camera, texture slots, blending modes, display list (record → playback); golden-image harness is a first-class deliverable | Golden-image pixel-diff within tolerance + display-list unit tests, all four targets | planned |
| F3 | 3D core | Camera, mesh slots, `drawMesh`, matrix math, depth test, vertex colors, procedural primitives | Golden images + math unit tests | planned |
| F4 | Lighting + Phong (F4a/F4b) | 4 point + 1 directional light, 4-channel Phong on solids/vertex colors (F4a); per-channel maps + alpha masks (F4b); canned-shader strategy settled here at the latest | Golden images + lighting unit tests against a CPU reference implementation | planned |
| F5 | Render targets + post FX | RTT, fullscreen-quad passes, color filter, blur | Golden images | planned |
| F6 | Resource packaging | Zip resource root, real asset import (asset format decided here), interactive REPL | Script tests load assets from a zip; REPL exercised via piped stdin | planned |
| F7 | Skinning + animation | CPU skinning into a mesh slot, skeleton/animation import, play/pause/blend | FK joint-transform tests vs CPU reference + golden images | planned |
| F8 | High-level JS + text | `drawModel`, `drawText` (font atlas built on quads), demo resource pack | Golden images; demo pack runs end-to-end on all four targets | planned |

Deferred cross-cutting decisions settle inside specific milestones, not before:
toolchain/QuickJS flavor/math library in F1, golden-image tolerance + CI
determinism in F2, canned-shader strategy by F4 at the latest, asset format in
F6.

## Non-negotiable design constraints (easy to get wrong)

- **Fixed-function pipeline only** — no programmable shaders, ever.
- Fixed limits: 4 point lights + 1 directional light, 1 camera.
- Immediate-mode *API*, but rendering goes through a re-orderable display list
  — do not map API calls 1:1 to draw calls.
- JS API layering: low/mid-level in C/C++ (`drawQuad`, `drawMesh`,
  `setMaterial`…), high-level conveniences in pure JS (`drawModel`,
  `drawText`…).
- JS code must have **zero browser/Node dependencies, not even transitively**.
- Memory rules: manage resources in JS where possible; unavoidable unmanaged
  resources are exposed as handles or pre-allocated slots
  (`setMesh(0, data); useMesh(0)`), to avoid leaks in a GC'd language.

## Reference implementations

Use these when designing, don't reinvent: sokol-samples (rendering patterns),
rayjs (QuickJS integration + stripping QuickJS for cross-platform).

## Not yet decided

Toolchain details, test strategy, CI, and repo layout are open questions —
settle them via OpenSpec proposals, not by silently picking defaults. The
Roadmap section above assigns each deferred decision a latest-settling
milestone.

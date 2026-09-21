# AGENTS.md

Guidance for agents working in this repo. The product source of truth is
`vision.md`; read it before proposing anything. Work flows through the
OpenSpec SDD flow — the `opsx-*` / `openspec-*` commands and skills
(propose → apply → archive) — rather than ad-hoc coding.

## Current state

- F1 (player skeleton) is **done**; F2 (2D layer) is **implemented, gate
  pending CI verification** — see `openspec/changes/f2-2d-layer`. The
  normative milestone ladder is `openspec/specs/feature-roadmap`; the
  table below summarizes it.
- `src/` is a single core static library (`platform`, `runtime`, `api`,
  `player`, `render`) plus a thin `main.c` (ADR 0003). Sokol and
  quickjs-ng are vendored pinned snapshots under `vendor/`
  (`vendor/README.md`, ADR 0006); stb is vendored for golden-image I/O.
- The `efx` player binary has two run modes (ADR 0007): windowed
  (`player <resource-root>`, runs `main.js`'s `update`/`render` hooks)
  and headless (`player --script <file> [args…]`, exit-code contract),
  plus a capture mode for golden images (`--capture-frame N
  --capture-output file`, ADR 0020).
- The script-facing API: F1's `efx.log`, `efx.quit`, `efx.args`, plus F2's
  2D layer — `setCamera2D` (virtual frame), `drawQuad`, `setBlendMode`,
  `setClearColor`, `createImageData`, `createTexture`, `whiteTexture`
  — cataloged in `docs/js-api.md` (F2 entries are current behavior).
- Verification: ctest runs smoke + headless display-list/JS-API unit tests
  everywhere; golden-image tests (6 committed scenes under
  `tests/goldens/`) run where a display exists — Linux CI under
  `xvfb-run` + llvmpipe, Emscripten in pinned headless Chrome
  (ADR 0020). Local builds without a display configure with
  `-DEFX_BUILD_GOLDEN_TESTS=OFF` (the default).
- `package.json` exists only to install the OpenSpec CLI. The
  `openspec` binary is not on PATH: run `npm install` once, then invoke
  commands as `npx openspec <command>` from the repo root (e.g.
  `npx openspec status --change <name>`, `npx openspec validate --strict`).

## Stack

- C11 core (ADR 0001); rendering via **Sokol** — fixed-function consumer
  API, no script-visible shaders ever (internals use Sokol's programmable
  pipeline with engine-owned canned shaders, ADR 0015);
  **quickjs-ng** embedded as the ES6
  runtime (ADR 0002); Emscripten bridge for the browser.
- Build system is CMake; targets: Windows, Linux, macOS, Emscripten;
  output is a single binary "player" for a resource folder/zip with a
  `main.js` entry (godot `res://`-style resource root).
- Math: **GLM**, integrated in F3 behind a plain C wrapper (ADR 0005).

## Roadmap

`vision.md` decomposes into a fixed ladder of milestones — the normative
spec is `openspec/specs/feature-roadmap`. The order is fixed: a milestone
must not start before its predecessor's verification gate passes on all
four targets, and every feature proposal must name the milestone it
implements.

| # | Milestone | Scope (one line) | Verification gate | Status |
|---|-----------|------------------|-------------------|--------|
| F1 | Player skeleton | CMake + vendored Sokol/QuickJS, window, resource root, `main.js` hooks, `--script` run mode | Builds on Win/Linux/macOS/Emscripten; script smoke test crosses the JS/C boundary and exits 0 on each | done |
| F2 | 2D layer | `drawQuad`, ortho camera, texture slots, blending modes, display list (record → playback); golden-image harness is a first-class deliverable | Golden-image pixel-diff within tolerance + display-list unit tests, all four targets | implemented, gate pending CI |
| F3 | 3D core | Camera, mesh slots, `drawMesh`, matrix math, depth test, vertex colors, procedural primitives | Golden images + math unit tests | planned |
| F4 | Lighting + Phong (F4a/F4b) | 4 point + 1 directional light, 4-channel Phong on solids/vertex colors (F4a); per-channel maps + alpha masks (F4b); canned-shader strategy settled here at the latest | Golden images + lighting unit tests against a CPU reference implementation | planned |
| F5 | Render targets + post FX | RTT, fullscreen-quad passes, color filter, blur | Golden images | planned |
| F6 | Resource packaging | Zip resource root, glTF 2.0 asset import — meshes, images, skins, animation clips (profile decided here), interactive REPL | Script tests load assets from a zip; REPL exercised via piped stdin | planned |
| F7 | Skinning + animation | CPU skinning into a mesh slot, skeleton/animation import, play/pause/blend | FK joint-transform tests vs CPU reference + golden images | planned |
| F8 | High-level JS + text | `drawModel`, `drawText` (font atlas built on quads), demo resource pack | Golden images; demo pack runs end-to-end on all four targets | planned |

Deferred cross-cutting decisions settle inside specific milestones, not
before: golden-image tolerance + CI determinism (incl. emsdk pinning) in
F2, canned-shader strategy by F4 at the latest, glTF import profile in
F6. F1's deferred set (toolchain, quickjs flavor, math library) is
settled — see `docs/decisions/`.

## Non-negotiable design constraints (easy to get wrong)

- **Fixed-function pipeline only** — no shader-shaped features on the
  consumer API, ever; the internal renderer uses Sokol's programmable
  pipeline with engine-owned canned shaders (ADR 0015).
- Fixed limits: 4 point lights + 1 directional light, 1 camera.
- Immediate-mode *API*, but rendering goes through a re-orderable display list
  — do not map API calls 1:1 to draw calls.
- JS API layering: low/mid-level in C/C++ (`drawQuad`, `drawMesh`,
  `setMaterial`…), high-level conveniences in pure JS (`drawModel`,
  `drawText`…).
- JS code must have **zero browser/Node dependencies, not even transitively**.
- Memory rules: manage resources in JS where possible; unavoidable unmanaged
  resources are exposed as GC-finalized opaque classes with explicit
  `destroy()` (textures, meshes, … — ADR 0011, discipline ADR 0012) or as
  fixed pre-allocated banks (lights), to avoid leaks in a GC'd language.
- Script-facing API changes require a `js-api` spec delta and a matching
  `docs/js-api.md` update in the same change (see `docs/js-api.md`).

## Documentation

- `vision.md` — product goals; the source of truth for intent.
- `docs/js-api.md` — the script-facing API catalog; updated in the same
  change as any API delta.
- `docs/decisions/` — architecture decision records (ADRs): the durable
  *why* behind cross-cutting invariants (language, runtime, module
  walls, binding pattern, vendoring, run modes, CI).
- `openspec/specs/` — required behavior; `openspec/changes/` — full
  design/process records per change.

Dividing rule: `openspec/specs/` pin required behavior, `docs/` hold
invariants and rationale, this file points rather than restates. When
work settles a durable architecture decision — a trade-off future
changes must respect — document it as a short ADR in `docs/decisions/`
(new numbered file + a row in its index). Full design/process records
stay in `openspec/changes/`; the ADR extracts only what outlives the
change.

## Reference implementations

Use these when designing, don't reinvent: sokol-samples (rendering patterns),
rayjs (QuickJS integration + stripping QuickJS for cross-platform).

## Not yet decided

Golden-image tolerance and CI determinism (F2), the canned-shader
strategy (by F4 at the latest), and the glTF import profile (F6) are
open — settle them via OpenSpec proposals, not by silently picking
defaults. The Roadmap section assigns each deferred decision a
latest-settling milestone. The glTF 2.0 import format itself is pinned
in the roadmap; only the profile remains open.

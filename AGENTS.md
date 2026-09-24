# AGENTS.md

Guidance for agents working in this repo. The product source of truth is
`vision.md`; read it before proposing anything. Work flows through the
OpenSpec SDD flow — the `opsx-*` / `openspec-*` commands and skills
(propose → apply → archive) — rather than ad-hoc coding.

## Current state

- F1 (player skeleton) and F2 (2D layer) are **done** — the F2 gate is
  verified green on the full four-target CI matrix (native 41/41 on
  Windows/Linux/macOS with all six golden scenes, Emscripten 23/23, web
  goldens all six scenes). The two follow-ups are archived too:
  `f2a-sokol-shdc` (D3D11/Metal quads via generated canned shaders, ADR
  0021) and `f2b-web-native-runtime` (the web player runs on the browser's
  native JS engine through the `src/web/` bridge, with no quickjs in the
  wasm, ADR 0022). The F2 change is archived at
  `openspec/changes/archive/2026-09-22-f2-2d-layer`.
- `src/` is a single core static library (`platform`, `runtime`, `api`,
  `player`, `render`) plus a thin `main.c` (ADR 0003). Sokol and
  quickjs-ng are vendored pinned snapshots under `vendor/`
  (`vendor/README.md`, ADR 0006); stb is vendored for golden-image I/O.
- The `efx` player binary has two run modes (ADR 0007): windowed
  (`player <resource-root>`, runs `main.js`'s `update`/`render` hooks)
  and headless (`player --script <file> [args…]`, exit-code contract),
  plus a capture mode for golden images (`--capture-frame N
  --capture-output file`, ADR 0020).
- The script-facing API: F1's `efx.log`, `efx.quit`, `efx.args`, and
  lifecycle `efx.registerUpdateHook` / `efx.registerRenderHook` (stacking,
  `dt`, unsubscribe; global `update`/`render` remain load-time sugar), plus
  F2's 2D layer — `setCamera2D` (virtual frame), `drawQuad`, `setBlendMode`,
  `setClearColor`, `createImageData`, `createTexture`, `whiteTexture`
  — cataloged in `docs/js-api.md` (F1/F2 entries are current behavior).
- Verification: ctest runs smoke + headless display-list/JS-API unit tests
  everywhere (on Emscripten the smoke suite runs the same portable scripts
  through the native bridge with the host JS engine as the runtime, plus
  `tools/run_web_compare.mjs` diffs desktop vs web output); golden-image
  tests (7 committed scenes under `tests/goldens/`) run where a display
  exists — Linux CI under `xvfb-run` + llvmpipe, Emscripten in pinned
  headless Chrome (ADR 0020). Local builds without a display configure with
  `-DEFX_BUILD_GOLDEN_TESTS=OFF` (the default); if a local build dir was
  configured with `ON`, exclude them (`ctest -E golden`) — goldens fail
  without a display.
- **CI runs on tags and manually, never per push (ADR 0023).**
  `.github/workflows/ci.yml` triggers only on `v*` tags and
  `workflow_dispatch` (`gh workflow run ci.yml`); ordinary branch pushes
  and pull requests do not start it. Every run publishes four downloadable
  archives (native player for Linux/Windows/macOS, Emscripten web bundle)
  as workflow artifacts, and a tag run attaches the same archives to that
  tag's GitHub Release. Use a manual run to prove the gate.
- **Pages deploys separately.** The public web player is built and
  deployed by `.github/workflows/pages.yml` on pushes to `main` and on
  manual dispatch — not by the gate workflow. A manual gate run on any
  ref therefore contains no deployment job and can be green.
- **CI verification order (all future changes): run the Linux pipeline
  first and fix anything it finds; only if Linux passes run the Windows
  pipeline; only if Windows passes run the macOS pipeline.** Linux is the
  fastest, cheapest signal (llvmpipe, matches the canonical goldens);
  Windows and macOS are slower per-roundtrip and verified in that order.
  The full matrix still gates every milestone (ADR 0020) — the order is
  about how changes are iterated, not about which targets count.
- **Verify on the SSH verification server BEFORE dispatching the gate.**
  A Linux server with Xvfb + llvmpipe, pinned emsdk 3.1.64 and pinned
  chrome-headless-shell 131 runs the exact two golden-bearing jobs the
  gate runs on ubuntu-latest (native ctest incl. all golden scenes, and
  the Emscripten golden suite). Flow: commit → push branch →
  `python3 tools/verify_remote.py all <branch>` → only if green dispatch
  `gh workflow run ci.yml --ref <branch>`. If the server verification
  fails, fix and re-verify — do not start a GitHub Actions run yet.
  This is a pre-filter for the Linux signal; the Linux→Windows→macOS
  order and the four-target gate still apply as above. Credentials come
  from the `SSH_HOST` / `SSH_USER` / `SSH_PASSWORD` env vars only — never
  commit them or the server's identity. Details and quirks:
  `docs/verification-server.md`. Known quirks: adding a golden scene
  requires a manual server-side capture before verification (llvmpipe
  only; recipe in that doc), and `ubuntu-latest` moves to Ubuntu 26 on
  2026-10-19, which may bump llvmpipe and require a golden re-baseline
  per ADR 0020.
- **Agents may commit and push to run the gate.** Because CI never fires on
  an ordinary push (ADR 0023), an agent verifying a change MAY create a
  branch, commit, and push for the sole purpose of dispatching
  `gh workflow run ci.yml --ref <branch>` — no separate commit/push request
  is needed for feature verification. Keep commits scoped to the change under
  verification and never sweep in unrelated working-tree changes.
- `package.json` exists only to install the OpenSpec CLI. The
  `openspec` binary is not on PATH: run `npm install` once, then invoke
  commands as `npx openspec <command>` from the repo root (e.g.
  `npx openspec status --change <name>`, `npx openspec validate --strict`).
  Known CLI noise: every command prints `Rules for 'design' must be an
  array of strings, ignoring this artifact's rules` even though
  `openspec/config.yaml` is well-formed — a CLI-side parse issue
  (f2c apply notes); honor the design rules by reading the config
  directly instead of chasing the warning.

## Stack

- C11 core (ADR 0001); rendering via **Sokol** — fixed-function consumer
  API, no script-visible shaders ever (internals use Sokol's programmable
  pipeline with engine-owned canned shaders, ADR 0015);
  **quickjs-ng** embedded as the desktop ES6 runtime (ADR 0002); on
  Emscripten the page's native JS engine drives the core through the
  `src/web/` bridge — no quickjs in the wasm (ADR 0022).
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
| F2 | 2D layer | `drawQuad`, ortho camera, texture slots, blending modes, display list (record → playback); golden-image harness is a first-class deliverable | Golden-image pixel-diff within tolerance + display-list unit tests, all four targets | done — full four-target CI matrix green; `f2a` (shdc) + `f2b` (web runtime) archived, ADR 0021/0022 |
| F3 | 3D core | Camera, mesh slots, `drawMesh`, matrix math, depth test, vertex colors, procedural primitives | Golden images + math unit tests | planned |
| F4 | Lighting + Phong (F4a/F4b) | 4 point + 1 directional light, 4-channel Phong on solids/vertex colors (F4a); per-channel maps + alpha masks (F4b); F4 lighting shaders reuse the sokol-shdc pipeline (strategy settled in F2, ADR 0021) | Golden images + lighting unit tests against a CPU reference implementation | planned |
| F5 | Render targets + post FX | RTT, fullscreen-quad passes, color filter, blur | Golden images | planned |
| F6 | Resource packaging | Zip resource root, glTF 2.0 asset import — meshes, images, skins, animation clips (profile decided here), interactive REPL | Script tests load assets from a zip; REPL exercised via piped stdin | planned |
| F7 | Skinning + animation | CPU skinning into a mesh slot, skeleton/animation import, play/pause/blend | FK joint-transform tests vs CPU reference + golden images | planned |
| F8 | High-level JS + text | `drawModel`, `drawText` (font atlas built on quads), demo resource pack | Golden images; demo pack runs end-to-end on all four targets | planned |

Deferred cross-cutting decisions settle inside specific milestones, not
before: golden-image tolerance + CI determinism (incl. emsdk pinning) in
F2 (done: tolerance/determinism + canned-shader strategy), glTF import profile in
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

Golden-image tolerance and CI determinism are settled (F2, ADR 0020),
as is the canned-shader strategy (settled early in F2 via
`f2a-sokol-shdc`, ADR 0021 — canned shaders are single-source GLSL in
`shaders/*.glsl`, compiled with pinned sokol-shdc). The glTF import
profile (F6) remains open — settle it via an OpenSpec proposal, not by
silently picking defaults. The Roadmap section assigns each deferred
decision a latest-settling milestone. The glTF 2.0 import format itself is pinned
in the roadmap; only the profile remains open.

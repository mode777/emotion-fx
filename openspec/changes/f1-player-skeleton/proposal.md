# Proposal

**Roadmap position:** Implements milestone **F1 (player skeleton)** from
`openspec/specs/feature-roadmap`. F1 is the first milestone and has no
predecessor gate, so this proposal is in roadmap order.

## Why

The project is pre-implementation: vision.md and the roadmap exist, but there is
no code. F1 delivers the two highest-risk foundations first, exactly as the
roadmap's "early risk retirement" requirement demands: the four-platform build
matrix (Windows, Linux, macOS, Emscripten) and the JS/C runtime boundary
(embedded QuickJS executing engine-exposed functions). Everything in F2–F8
stacks on these, so they must be proven — by actually building and running on
all four targets — before any rendering work starts.

## What Changes

- Introduce a CMake build system producing a **single binary "player"** on all
  four targets (Win, Linux, macOS, Emscripten).
- Vendor the first two third-party dependencies: **sokol** (rendering/platform
  layer) and **quickjs-ng** (ES6 interpreter on non-browser platforms).
- Create the player runtime skeleton: resource-root loading (folder on desktop
  platforms), `main.js` entry point with lifecycle hook pickup, a sokol window
  with a frame loop, and update/render callback dispatch into JS.
- Add a `--script <file>` run mode that executes a single JS file without
  opening a window and propagates its exit code — the vehicle for smoke tests.
- Expose the first C-ABI-bound JS functions (a minimal, non-rendering API
  surface such as print/logging and quit) to prove the JS→C→JS crossing.
- Record the deferred F1 decisions (binding per the roadmap's
  "latest-settling" rule): core language **C11**, QuickJS flavor
  **quickjs-ng**, math library **GLM** (decision only; GLM is not integrated in
  F1 — first use is F3).
- Establish the smoke-test harness convention (scripted tests with exit-code
  assertions) that F2's golden-image harness will later join.
- Add a GitHub Actions workflow that runs the four-platform build matrix and
  the smoke suite on every push (added during apply: local container builds
  for all four toolchains proved impractical; CI is the F1 gate runner).

Non-goals: no rendering API (F2+), no 3D/math (F3), no lighting (F4), no render
targets/post FX (F5), no zip packaging/REPL (F6), no skinning (F7), no
high-level JS API/text (F8). Golden-image CI determinism remains F2 scope; F1's
CI covers build + headless smoke tests only.

## Capabilities

### New Capabilities

- `build-system`: CMake targets and vendored-dependency rules producing the
  single player binary on Windows, Linux, macOS, and Emscripten.
- `player-runtime`: player executable behavior — resource root (folder)
  loading, `main.js` hook pickup, window + frame loop, run modes
  (resource-root mode and `--script` mode), and exit-code contract.
- `js-runtime`: embedded quickjs-ng execution — ES6 script evaluation, C-ABI
  function binding mechanism, zero browser/Node dependencies (also
  transitively), and error/exit-code propagation.
- `verification`: the smoke-test harness — scripted JS/C-boundary tests with
  exit-code assertions that must pass on all four targets (the F1 gate), as the
  foundation the F2 golden-image harness extends.

### Modified Capabilities

None. `feature-roadmap` already describes F1's position and gate; this change
implements it without altering roadmap requirements.

## Impact

- **New code/repo layout:** first source tree (`src/`), vendored third-party
  code (`vendor/sokol`, `vendor/quickjs-ng`), CMake files, a sample/test
  resource root with `main.js`, and smoke-test scripts (`tests/`).
- **Dependencies:** sokol and quickjs-ng become vendored, pinned dependencies;
  GLM is recorded as the future math dependency (introduced in F3).
- **APIs:** establishes the C ABI + JS binding pattern (`efx_*` C functions
  registered into the QuickJS global) that every later engine function will
  follow; establishes the `main.js` hook contract (`update`/`render`) that F2+
  builds on.
- **Platforms:** all four targets must build and run smoke tests from F1
  onward; a broken target blocks the ladder per the roadmap.
- **Existing code:** none — the repository currently has no source.

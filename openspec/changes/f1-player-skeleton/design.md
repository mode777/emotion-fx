# Design

## Context

Greenfield: the repository has no source. See proposal.md for motivation and
the four delta specs for the behavior contract. Constraint set comes from
vision.md and the roadmap: fixed-function rendering arrives later via Sokol;
the ES6 consumer API must be dependency-free of browser/Node; the F1 gate is
the four-platform build matrix plus JS/C-boundary smoke tests with exit-code
checks.

The user settled F1's three deferred decisions up front:

| Decision | Choice |
|---|---|
| Core language | **C11** (JS-facing API is a C ABI regardless) |
| ES6 interpreter | **quickjs-ng**, vendored and pinned |
| Math library | **GLM** — decision recorded now, not integrated in F1 (first use F3) |

## Goals / Non-Goals

**Goals:**

- A code skeleton so clean that F2–F8 features bolt on without reworking the
  foundations: one core static library, one thin `main`, one binding pattern.
- Headless `--script` mode as the automation surface — everything the gate
  needs must run without a window or human input.
- The Emscripten build shares the same C core; only the platform/host layer
  differs.

**Non-Goals:**

- No rendering API, math, lighting, RTT, zip/REPL, skinning, or high-level JS
  API (F2–F8 scope).
- No CI pipelines (roadmap settles CI in F2); the F1 gate is run as a local
 /scripted matrix.
- No automated window/frame-loop tests on headless machines — verified
  manually per platform in F1 (see Risks).

## Decisions

### D1: Module layout — core library + thin main
`src/` is a single static library with four internal modules, plus a thin
`main.c`:

- `platform` — sokol wrapper: window creation, frame callback, clear pass.
  The only module that may include sokol headers.
- `runtime` — JS engine host: context create/destroy, global-namespace
  registration, script evaluation, hook pickup/dispatch, error extraction.
  The only module that includes quickjs headers.
- `api` — the engine functions exposed to JS (`efx_log`, `efx_quit`,
  `efx_args` in F1). Pure C, no quickjs types leak past thin trampoline
  functions.
- `player` — CLI parsing, run-mode selection, exit-code plumbing.

*Why:* isolating sokol and quickjs behind module walls means the F4 canned
shader strategy or a future interpreter swap touch one module each.
*Alternative considered:* one flat `main.c` — rejected; F2+ would tangle
immediately.

### D2: Binding pattern — single `efx` global namespace
All engine functions are registered as C callbacks on one global object,
`efx` (e.g. `efx.log("hi")`, `efx.quit(3)`, `efx.args()`). Every future
low/mid-level function (`drawQuad`, `setMaterial`, …) follows this pattern;
high-level conveniences stay in pure JS per vision.md layering.

*Why quickjs-ng over bellard/quickjs:* actively maintained, CMake-native
build, near-compatible C API with bellard's (so sokol-samples/rayjs reference
knowledge mostly transfers), and it builds under Emscripten. *Trade-off:*
vision.md references bellard's repo; the divergence is contained inside the
`runtime` module (D1), so a swap stays localized.

*Memory rule:* engine resources exposed to JS follow the roadmap constraint —
pre-allocated slots/handles, never GC-managed C objects. F1 has no such
resources, but the pattern is stated now so F2+ inherits it.

### D3: Run modes and exit codes
`player <resource-root>` → evaluate `main.js`, open the sokol window, run the
frame loop (each frame: C dispatches JS `update`, then JS `render`; sokol
pass-action clears to a fixed color), exit 0 on window close or on
`efx.quit(0)`. Hooks are looked up once after evaluation; missing ones are
skipped. `player --script <file> [args…]` → no sokol initialization at all;
evaluate, propagate `efx.quit(n)` as process exit code.

Exit codes: `0` success; `1` generic runtime failure (missing root/entry
script, uncaught exception, file-not-found); otherwise the script-requested
code. Uncaught exceptions and the error object's message/stack go to stderr
via quickjs exception dumping.

*Why headless `--script`:* the gate must run on machines with no display.
*Alternative considered:* windowed script mode — rejected for F1 (no draw API
exists to justify it).

### D4: Emscripten smoke tests run under Node
The wasm player's `--script` mode never touches browser APIs, so its
Emscripten output (JS glue + wasm) runs under plain Node.js in the test
harness. This does not violate the "no Node dependencies" rule — that rule
binds game scripts (which stay pure ES6); Node is only the test *launcher*,
equivalent to a shell. `ctest` drives all four targets; the Emscripten test
wraps the invocation in Node. *Fallback if Node-run proves unworkable:*
headless Chromium via Playwright — heavier, last resort.

*Frame-loop smoke checks* that need a real window are a documented manual
checklist per platform in F1; F2's golden-image harness automates rendering
verification.

### D5: Vendoring — pinned source snapshots in-repo
`vendor/sokol/` (the handful of needed single headers) and
`vendor/quickjs-ng/` (release snapshot) are committed to the repo, with
`vendor/README.md` recording source and pinned version. No FetchContent,
no submodules, no network at configure/build time.

*Alternatives considered:* CMake FetchContent (violates the offline-build
requirement), git submodules (extra setup step and a recurring source of
agent friction). *Trade-off:* manual version bumps; acceptable at this
dependency count.

### D6: GLM recorded, not integrated (C11/GLM interop note)
GLM is the engine's math library decision from F1, but GLM is C++-only and the
core is C11. When F3 introduces it, GLM usage will live in small C++-compiled
translation units exposing a plain C API to the core (same module-wall pattern
as D1) — or the decision is revisited via a change proposal if that wrapper
cost proves unjustified. F1 ships no math code and no GLM sources.

### D8: GitHub Actions is the gate runner (added during apply)
The F1 gate runs as CI: a workflow matrix builds on `ubuntu-latest`,
`windows-latest`, `macos-latest` (apt-installed X11/GL dev packages on Linux)
plus a separate Emscripten job (emsdk action), and runs the ctest smoke suite
on every target — the suite is headless, so it needs no display anywhere.
Rationale: the development container hosts none of the four toolchains
faithfully (no MSVC/Xcode, no X11, emsdk needs ~3 GB the state volume does
not have), while CI runners have all of them natively; the user directed this
pivot during apply. Windowed runtime verification remains a manual per-platform
checklist (D4). *Trade-off:* the Emscripten job tracks `latest` emsdk for F1;
pinning settles in F2 with golden-image CI determinism, as the roadmap
assigns.

### D7: Toolchain floor
CMake ≥ 3.21; C11; compilers: MSVC 2022 (Windows), clang/gcc (Linux/macOS),
Emscripten ≥ 3.1 (latest LTS line). quickjs-ng built as a CMake static lib;
sokol headers compiled per-platform via its standard implementation-macro
pattern in one `platform` TU. Warnings-as-errors on the core (not on vendor
code).

## Risks / Trade-offs

- [sokol pulled into Emscripten build even for `--script`] → keep `platform`
  module out of the headless code path; if sokol's emscripten glue interferes
  with Node execution, compile the platform TU out of the headless entry point
  (link-level separation).
- [quickjs-ng API drift vs bellard examples] → pin a release; isolate all
  interpreter calls in `runtime`; verify the three F1 API functions first —
  they exercise the binding pattern end-to-end before anything stacks on it.
- [Frame-loop behavior only manually verified in F1] → explicit per-platform
  manual checklist in tasks; automated coverage arrives with F2's golden-image
  harness (roadmap-assigned).
- [GLM/C11 wrapper cost surfaces in F3] → decision D6 names both the wrapper
  approach and the revisit path; no sunk cost in F1.
- [stdout/stderr flushing differences across targets can garble test output] →
  test scripts assert via exit codes only; logs are diagnostic, never
  assertions (except `efx.log` crossing tests, which use a trailing flush
  before exit).

## Migration Plan

Greenfield — nothing to migrate. Implementation lands as the first source
commit: vendor snapshots, CMake, `src/`, `examples/hello/` resource root,
`tests/` smoke suite. Rollback is reverting the commit. The gate procedure
(build all four targets, run `ctest` on all four, manual window checklist)
goes in the repo README as part of tasks.

## Open Questions

None blocking. CI design and golden-image tolerance are F2-assigned; zip
packaging and REPL are F6-assigned; the `efx` namespace name is settled here
and trivially renameable later if desired.

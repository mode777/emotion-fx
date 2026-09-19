# EmotionFX

An old-school, PS2-era 3D engine: fixed-function pipeline, super lightweight,
scripted in ES6. See `vision.md` for the product vision and
`openspec/specs/feature-roadmap` for the milestone ladder. Current status:
**F1 (player skeleton)**.

## What F1 delivers

- A single-binary **player** built with CMake for Windows, Linux, macOS, and
  Emscripten.
- Embedded **quickjs-ng** ES6 runtime on every target (browser builds run the
  same core through Emscripten).
- A resource root (folder) loaded from the command line, with a `main.js`
  entry script providing `update`/`render` frame hooks.
- A headless `--script` run mode that executes one script and propagates its
  exit code (the automated-test vehicle).
- The engine JS API namespace `efx`: `efx.log(msg)`, `efx.quit(code)`,
  `efx.args()` — the binding pattern all future engine functions follow.

## Repository layout

```
src/            C11 core (api, runtime, player, platform modules)
vendor/         vendored pinned dependencies (sokol, quickjs-ng)
tests/          ctest smoke suite + temporary dev harness
examples/       sample resource roots
openspec/       OpenSpec specs and change artifacts
```

## Building

Requirements: CMake ≥ 3.21 and a C11 toolchain.

| Target | Commands |
|---|---|
| Linux | `sudo apt install libx11-dev libxi-dev libxcursor-dev libgl1-mesa-dev` then `cmake -B build && cmake --build build` |
| Windows (VS 2022) | `cmake -B build && cmake --build build --config Release` |
| macOS (Xcode toolchain) | `cmake -B build && cmake --build build` |
| Emscripten | `emcmake cmake -B build-em && cmake --build build-em` |

## Running

```sh
build/player examples/hello          # window + frame loop
build/player --script tests/scripts/s_quit3.js   # headless; exits 3
```

## Testing / the F1 gate

The smoke suite is ctest-based and headless (`--script` mode), so it runs on
every target including Emscripten (under Node):

```sh
ctest --test-dir build -C Release --output-on-failure
```

CI (`.github/workflows/ci.yml`) runs the build matrix and the full suite on
ubuntu, windows, macOS, and Emscripten on every push to `main`. A red run
blocks the roadmap ladder per `feature-roadmap`.

Window behavior (window opens, hooks run per frame, clean exit on close) is
verified manually per desktop platform for F1 — CI runners have no display.
Checklist: launch `build/player examples/hello`, confirm a 1024x600 window
with a dark blue-grey clear color, frame logs on stdout, clean exit 0 on
close after the 60-frame auto-quit (or immediately on manual close).

## Notes

- Dependencies are vendored and pinned in `vendor/` (see `vendor/README.md`);
  builds are fully offline.
- sokol's Linux backend needs X11/GL dev packages at build time; no display
  is needed for headless runs and tests.
- CI tracks `latest` emsdk for F1; pinning the Emscripten toolchain happens
  in F2 together with golden-image determinism.

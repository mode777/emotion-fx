# EmotionFX

An old-school, PS2-era 3D engine: fixed-function pipeline, super lightweight,
scripted in ES6. See `vision.md` for the product vision and
`openspec/specs/feature-roadmap` for the milestone ladder. Current status:
**F2 (2D layer) done; gate verified via CI**.

## What F2 delivers (on top of F1)

- The **2D drawing layer**: a virtual-pixel projection frame
  (`efx.setCamera2D`), `efx.drawQuad` with derived size (`size` /
  `sourceRect` / texture pixels), tint / rotation / scale / `origin` pivot,
  CPU→GPU textures (`createImageData`, `createTexture` — textures expose
  read-only `width`/`height`), blending modes (`alpha`, `additive`,
  `subtractive`), and the engine-owned `efx.whiteTexture` for solid rects.
- The **re-orderable display list** between the immediate-mode API and sokol
  (ADR 0019), unit-tested headlessly (record → assert, no GPU).
- The **golden-image verification harness** (ADR 0020): capture run mode
  (`--capture-frame N --capture-output file`), committed PNG goldens under
  `tests/goldens/`, tolerance comparator (`tests/imgdiff.c`), and
  software-rendered, toolchain-pinned CI jobs.

- A single-binary **player** built with CMake for Windows, Linux, macOS, and
  Emscripten.
- Embedded **quickjs-ng** ES6 runtime on desktop; on Emscripten the browser's
  native JS engine drives the same core through the `src/web/` bridge, with no
  quickjs in the wasm (ADR 0022).
- A resource root (folder) loaded from the command line, with a `main.js`
  entry script providing `update`/`render` frame hooks.
- A headless `--script` run mode that executes one script and propagates its
  exit code (the automated-test vehicle).
- The engine JS API namespace `efx`: `efx.log(msg)`, `efx.quit(code)`,
  `efx.args()` — the binding pattern all future engine functions follow.
## JavaScript API

The normative script-facing API reference — current behavior plus the
provisional F2–F8 catalog — lives in [`docs/js-api.md`](docs/js-api.md).

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

## Testing / the F2 gate

The suite is ctest-based: headless smoke tests (`--script` mode), headless
display-list + JS-API unit tests (mock GPU sink, no window needed), and —
where a GPU/display exists — golden-image capture tests:

```sh
cmake -B build -DEFX_BUILD_GOLDEN_TESTS=ON
cmake --build build
ctest --test-dir build -C Release --output-on-failure
```

`EFX_BUILD_GOLDEN_TESTS` defaults OFF (CI turns it on; Linux CI renders
under `xvfb-run` with `LIBGL_ALWAYS_SOFTWARE=1`). The Emscripten job runs
its goldens through pinned headless Chrome with SwiftShader
(`tools/run_web_goldens.mjs`).

**Regenerating goldens** — only when intended output changed:

```sh
cmake -B build -DEFX_BUILD_GOLDEN_TESTS=ON && cmake --build build
./build/player --capture-frame 2 --capture-output tests/goldens/<scene>/golden.png tests/goldens/<scene>
```

Review the regenerated `golden.png` carefully before committing: goldens are
the reference, so a diff here is a deliberate rendering change. CI fails if
the toolchain or a code change alters output without a committed regen.

Window behavior (window opens, hooks run per frame, clean exit on close) is
verified manually per desktop platform — CI runners have no real display.
Checklist: launch `build/player examples/hello`, confirm a window opens with
frame logs on stdout and clean exit 0 on close after the 60-frame auto-quit.

## Continuous integration

CI is the four-target gate (Windows, Linux, macOS, Emscripten). It is
**not** run on every push: the workflow triggers on version tags (`v*`)
and on manual dispatch only (ADR 0023). The smoke suite, golden-image
checks, and web comparison harness all run inside those triggered runs.

```sh
gh workflow run ci.yml            # start the full gate on the current ref
gh run list --workflow ci.yml     # list runs and their status
```

Every completed run publishes four downloadable archives — the native
player for Linux, Windows, and macOS, plus the Emscripten web bundle
(HTML + JS + wasm + data) — under the run's **Artifacts**. A run triggered
by a `v*` tag additionally creates a GitHub Release for that tag with the
same four archives attached, so tagged versions are directly downloadable
from the release page. Manual runs use a `dev-<sha>` version token in the
archive names; tag runs use the tag name.

## Notes

- Dependencies are vendored and pinned in `vendor/` (see `vendor/README.md`);
  builds are fully offline.
- sokol's Linux backend needs X11/GL dev packages at build time; no display
  is needed for headless runs and tests.
- The Emscripten toolchain is pinned to an exact emsdk version in
  `.github/workflows/ci.yml` (golden-image determinism, ADR 0020).

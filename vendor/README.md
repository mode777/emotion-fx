# Vendored dependencies

All third-party dependencies are vendored in-repo at pinned versions; the
build never touches the network (see `build-system` spec). Update pins by
replacing the snapshot and editing the table below.

| Path | Project | Pinned version | Source |
|---|---|---|---|
| `sokol/` | floooh/sokol | master @ `2e75443dbd4940b5aa8d76a8e479f8e4b270b9a3` | https://github.com/floooh/sokol (only `sokol_app.h`, `sokol_gfx.h`, `sokol_glue.h`) |
| `quickjs-ng/` | quickjs-ng/quickjs | v0.17.0 (QJS 0.17.0) | https://github.com/quickjs-ng/quickjs, release tarball `v0.17.0.tar.gz` |
| `stb/` | nothings/stb | master @ `2c980bb59875b0d32144a71867fbdebb2f77cd20` (`stb_image` v2.30, `stb_image_write` v1.16) | https://github.com/nothings/stb (only `stb_image.h`, `stb_image_write.h`) |
| `glm/` | g-truc/glm | 1.0.3 @ `8d1fd52e5ab5590e2c81768ace50c72bae28f2ed` | https://github.com/g-truc/glm (core headers + `detail/` + `simd/` + `ext/` + `gtc/`; excludes `gtx/`, the C++20 module `glm.cppm`, `CMakeLists.txt`, umbrella `ext.hpp`) |
| — (tool, not vendored) | floooh/sokol-tools-bin | master @ `11d0cf678105d614d675e6d9bd2aaf3eeff12f8c` (2026-08-29) | https://github.com/floooh/sokol-tools-bin (`bin/linux/sokol-shdc`) — generation-time tool for `shaders/quad.h`; never linked into the player |

Notes:

- sokol is a rolling project without release tags; the pin is a master commit
  SHA. Re-pin by downloading the new commit's `sokol_app.h` / `sokol_gfx.h`.
- Only the sokol headers F1 needs are vendored (`sokol_app.h` for the window /
  frame loop, `sokol_gfx.h` for the clear pass). Add further sokol headers
  from the same pinned commit when a milestone needs them.
- quickjs-ng is consumed via its own CMake target (built as a static library,
  tests/examples/CLI/install disabled). The engine does not compile
  quickjs-libc into the runtime — scripts get only the engine's `efx` API plus
  the ES6 standard library, keeping them free of host (browser/Node) APIs.
- GLM (F3, ADR 0005): header-only C++; consumed only from C++-compiled
  translation units (`src/math/`) that expose a plain C API — GLM types never
  enter C11 translation units. The wrapper calls the convention-explicit
  `glm::perspectiveRH_NO` / `glm::orthoRH_NO` / `glm::lookAtRH` (right-handed,
  OpenGL depth range −1..+1); the `GLM_FORCE_*` defines are not used. The
  projection conventions for D3D11/Metal (0..1 depth) are handled at playback
  by the platform layer, not by flipping GLM conventions. Evaluation record:
  `openspec/changes/f3-3d-core/proposal.md`.
- sokol-shdc (ADR 0021) compiles `shaders/quad.glsl` into the committed
  `shaders/quad.h`. The tool is used at author time only; the player
  build never needs it (offline policy intact). Regeneration: download
  the pinned sokol-tools-bin revision, run
  `sokol-shdc -i shaders/quad.glsl -o shaders/quad.h --slang glsl410:glsl300es:hlsl4:metal_macos -f sokol_impl`,
  and commit the diff. Source-snapshot vendoring was evaluated and
  rejected: sokol-tools has no CMake build and pulls an 8-submodule
  dependency graph (glslang, SPIRV-Tools/Cross, tint, ...) for a
  generation-time-only tool.
- stb is vendored for golden-image PNG I/O (F2 verification harness):
  `stb_image_write` encodes captured frames, `stb_image` decodes committed
  goldens for comparison. Both are single-header public-domain/MIT; the
  implementation TUs live in the tools that need them (`tests/imgdiff.c`,
  `src/platform/capture.c`). Evaluation record:
  `openspec/changes/f2-2d-layer/proposal.md`.

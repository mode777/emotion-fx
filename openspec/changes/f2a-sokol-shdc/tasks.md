# Tasks

Implements the sokol-shdc migration completing F2's native rendering.
Decision record: `docs/decisions/0021` (written with this proposal).
Behavior gates: the native golden jobs per the `verification` spec.

## 1. Tool vendoring and build integration

- [ ] 1.1 Vendor the pinned sokol-tools source snapshot into `vendor/sokol-tools/`, record source repo + pin + license in `vendor/README.md`; verify `shdc` builds as a CMake host-tool target on the Linux CI host and emits `--help`
- [ ] 1.2 Add the CMake integration: host-tool target, `add_custom_command` generating `shaders/quad.h` from `shaders/quad.glsl`, and a CI check that regenerating produces no diff; verify a clean configure + build works offline

## 2. Single shader source

- [ ] 2.1 Write `shaders/quad.glsl` carrying the existing quad vertex/fragment stages in sokol GLSL conventions (attributes, sampler, program block per D2); verify the generated header contains GLSL330, HLSL and MSL flavors
- [ ] 2.2 Replace `VS_SRC`/`FS_SRC` and the manual `sg_shader_desc` wiring in `src/platform/pipeline.c` with the generated shader include; verify the Linux golden jobs still pass (GL flavor equivalence)

## 3. Native golden gates

- [ ] 3.1 Verify the Windows (D3D11/WARP) golden job passes — quads render, all six goldens within tolerance
- [ ] 3.2 Verify the macOS (Metal) golden job passes — quads render, all six goldens within tolerance

## 4. Docs

- [ ] 4.1 Add an addendum note to `openspec/changes/f2-2d-layer/design.md` marking the hand-written shader decisions (D2 blend pipelines, entry points) as superseded by the generated pipeline
- [ ] 4.2 Update the AGENTS.md canned-shader note (sokol-shdc adopted early; F4 shaders reuse the pipeline) and confirm `docs/decisions/0021` is indexed in `docs/decisions/README.md`

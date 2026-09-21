# Architecture decisions

Short, numbered records of durable architecture decisions — the **why**
behind invariants that span multiple modules, formats, or milestones.
Behavior requirements live in `openspec/specs/`; the per-change process
records (proposals, full design docs) live in `openspec/changes/`; this
directory holds only what stays true after a change is archived.

## Index

| # | Status | Decision |
| - | ------ | -------- |
| [0001](0001-c11-core-with-a-c-abi.md) | Accepted | C11 core with a C ABI for the script-facing API |
| [0002](0002-quickjs-ng-as-the-es6-runtime.md) | Accepted | quickjs-ng is the embedded ES6 runtime, without quickjs-libc |
| [0003](0003-module-walls-around-sokol-and-quickjs.md) | Accepted | Sokol and quickjs live behind module walls (platform/runtime/api/player) |
| [0004](0004-single-efx-global-namespace.md) | Accepted | Every engine function hangs off one global `efx` namespace |
| [0005](0005-glm-behind-a-plain-c-api.md) | Accepted | GLM is the math library, wrapped behind a plain C API (first use F3) |
| [0006](0006-vendored-pinned-source-snapshots.md) | Accepted | Dependencies are pinned source snapshots vendored in-repo |
| [0007](0007-headless-script-mode-exit-codes.md) | Accepted | Headless `--script` mode with an exit-code contract is the automation surface |
| [0008](0008-node-as-test-launcher-only.md) | Accepted | Node is a test launcher, never a script dependency |
| [0009](0009-github-actions-gate-runner.md) | Accepted | GitHub Actions is the four-target gate runner |
| [0010](0010-script-math-is-plain-js-data.md) | Accepted | Script math is plain JS data; GLM math stays behind the C wall |
| [0011](0011-dynamic-resources-are-gc-finalized-classes.md) | Accepted | Dynamic-count resources are GC-finalized opaque classes; slots only for fixed banks |
| [0012](0012-native-memory-gc-discipline.md) | Accepted | Native memory counts toward GC pressure; destroy-first, finalizer-backstop discipline |
| [0013](0013-resource-taxonomy-eight-opaque-types.md) | Superseded by 0014 | Resource taxonomy: eight GC-finalized opaque types |
| [0014](0014-skinning-follows-gltf-data-model.md) | Accepted | Skinning follows the glTF data model; seven resource types (weights in MeshData, Skeleton ≈ glTF skin) |
| [0015](0015-fixed-function-is-consumer-api-contract.md) | Accepted | Fixed-function is a consumer-API contract; internals use Sokol's programmable pipeline with canned shaders |
| [0016](0016-explicit-hook-registration-implicit-init.md) | Accepted | Lifecycle via explicit stacking hook registration; loading main.js is the implicit init |
| [0017](0017-implicit-rig-payload-skinned-flag.md) | Accepted | Skins and skeletons are implicit Mesh payload; `skinned` is a drawMesh flag (5 resource types) |
| [0018](0018-script-driven-posing.md) | Accepted | Script-driven posing via `poseMesh`; no engine playback state |

## Adding a decision

Copy [`TEMPLATE.md`](TEMPLATE.md) to `NNNN-short-slug.md`, fill it in,
and add a row to the index. Keep it short — context, decision,
consequences, rejected alternatives. Do not restate behavior that
`openspec/specs/` already pins down; link instead. Supersede by marking
the old entry Superseded and linking forward; do not delete accepted
records.

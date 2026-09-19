# Vendored dependencies

All third-party dependencies are vendored in-repo at pinned versions; the
build never touches the network (see `build-system` spec). Update pins by
replacing the snapshot and editing the table below.

| Path | Project | Pinned version | Source |
|---|---|---|---|
| `sokol/` | floooh/sokol | master @ `2e75443dbd4940b5aa8d76a8e479f8e4b270b9a3` | https://github.com/floooh/sokol (only `sokol_app.h`, `sokol_gfx.h`, `sokol_glue.h`) |
| `quickjs-ng/` | quickjs-ng/quickjs | v0.17.0 (QJS 0.17.0) | https://github.com/quickjs-ng/quickjs, release tarball `v0.17.0.tar.gz` |

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
- GLM is a recorded future dependency (math decision from F1); it is
  deliberately NOT vendored yet — first use is F3 (see
  `openspec/changes/f1-player-skeleton/design.md`, D6).

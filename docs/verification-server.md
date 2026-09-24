# Linux verification server (pre-CI gate)

Status: active workflow aid (2026-09). Policy owner: AGENTS.md
("Pre-CI verification server"). Steering script:
`tools/verify_remote.py`.

## Purpose

The local container has no display, no Xvfb and no Emscripten, so golden
images cannot be generated or verified locally (ADR 0020 default:
`-DEFX_BUILD_GOLDEN_TESTS=OFF`). The verification server is a Linux box
provisioned to run exactly the two golden-bearing jobs the GitHub Actions
gate runs on `ubuntu-latest`:

1. **native** — full build + ctest (44 tests, including all 6 golden
   scenes: `clear`, `solid`, `srcrect`, `pivot`, `blend`, `camera`) under
   Xvfb with Mesa llvmpipe (`LIBGL_ALWAYS_SOFTWARE=1`), the canonical
   golden configuration.
2. **web** — Emscripten capture build (`player_web_golden`) with the
   pinned emsdk, compared via `efx_imgdiff` against the committed
   goldens in pinned `chrome-headless-shell` (SwiftShader WebGL) —
   the same setup as the `golden-web` job in `ci.yml`.

**Policy: the server suites must pass before the GitHub Actions gate is
dispatched. If verification on the server fails, do not run
`gh workflow run ci.yml`.** This does not change the CI iteration order
(Linux pipeline first, then Windows, then macOS) — the server is a free,
fast pre-filter for the Linux signal, not a replacement for the
four-target gate (ADR 0020).

## Credentials and access

Access is purely via environment variables; **no endpoint, username or
password is stored in this repository**:

- `SSH_HOST` — server hostname
- `SSH_USER` — SSH user
- `SSH_PASSWORD` — SSH password

If any of them is unset, `tools/verify_remote.py` exits with a clear
error and touches nothing. Requires `paramiko`
(`pip3 install --user paramiko`) and a local `git` checkout with an
`origin` remote.

## Usage

```sh
# normal flow: commit your change, push the branch, then:
python3 tools/verify_remote.py native        # native golden suite
python3 tools/verify_remote.py web           # emscripten golden suite
python3 tools/verify_remote.py all           # both, native first

# then, only if green:
gh workflow run ci.yml --ref <branch>
```

`ref` defaults to the current branch and must be pushed — the script
verifies the pushed state via `git fetch` + `git reset --hard` into the
remote checkout (`~/emotion-fx` by default, `--dir` to override), which
keeps the server's warm build caches valid between runs. The script
warns if your local HEAD differs from the pushed branch. Exit code 0
means every requested suite passed.

## Server capabilities

| Component | Version / notes |
|---|---|
| OS | Ubuntu 24.04, 3 cores, ~3.7 GB RAM, ~10 GB free disk |
| CMake / gcc | 3.28.x / 13.3 — builds the native player fine |
| Xvfb + Mesa | `xvfb-run`, llvmpipe via `LIBGL_ALWAYS_SOFTWARE=1` |
| X/GL dev libs | `libx11-dev`, `libxi-dev`, `libxcursor-dev`, `libgl1-mesa-dev` — all installed |
| Emscripten | emsdk **3.1.64** — the exact CI pin (ADR 0020), at `/opt/emsdk` (plus copies under `~/emsdk`, `~/tools/emsdk`) |
| Chrome for goldens | `chrome-headless-shell` **131.x** under `~/browsers/` — matches the CI major-version pin; installed by the script if missing |
| Chromium (snap) | present but **not usable** for the golden driver (snap + old-headless don't cooperate); always use `chrome-headless-shell` |
| Node | v20 — runs `tools/run_web_goldens.mjs` and the npm deps |

## Quirks (all handled by `tools/verify_remote.py`)

- **`emcc` is not on PATH.** Activate emsdk first:
  `source /opt/emsdk/emsdk_env.sh` (it activates `~/tools/emsdk`, also
  3.1.64, and prints harmless cache noise).
- **`CHROME_SHELL_PATH` must point at `chrome-headless-shell`** —
  `run_web_goldens.mjs` requires it explicitly
  (`~/browsers/chrome-headless-shell/<version>/chrome-headless-shell-linux64/chrome-headless-shell`).
- **Node modules for the web driver are not preinstalled** in a fresh
  checkout: `npm install --no-save puppeteer-core @puppeteer/browsers`.
- **Golden invocations need the display wrappers**: `xvfb-run -a` +
  `LIBGL_ALWAYS_SOFTWARE=1` for native; `run_web_goldens.mjs` self-drives
  BeginFrames so nothing extra is needed there.
- **Small machine**: keep builds at `-j3`/`-j4`; a cold Emscripten build
  takes several minutes, warm rebuilds are fast — prefer the ref-sync
  flow (reuses `build*/` caches) over wiping the checkout.
- The remote checkout may lag `origin` or sit on an old branch; the
  script's sync step (fetch + hard reset to the verified SHA) makes that
  harmless.
- **Adding a golden scene requires a server-side capture first** (f2c,
  2026-09). A scene's test only activates once `golden.png` is committed,
  and the web golden driver fails on scenes whose PNG is missing — but
  captures need llvmpipe, which only exists here. `verify_remote.py` has
  no capture suite, so the flow is: push the branch without the PNG →
  over SSH: sync the checkout to the pushed SHA, build the native player,
  run `xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 ./build/player
  --capture-frame 2 --capture-output <out>.png tests/goldens/<scene>`
  (twice, imgdiffing the two captures as a determinism check), SFTP the
  PNG into the repo, commit, push — and only then `verify_remote.py all`.
- **CI runner watch item**: `ubuntu-latest` migrates to Ubuntu 26 from
  2026-10-19 (GitHub runner-images notice, observed 2026-09). That may
  bump Mesa/llvmpipe in the canonical `build+test (ubuntu-latest)` golden
  job; this server's llvmpipe is pinned only by the distro. If goldens
  drift after the migration, apply ADR 0020's tolerance/re-baseline rules
  and re-capture here so both golden jobs stay on the same rasterizer
  generation.

## Security notes

- Credentials live only in the environment of the calling shell; the
  script reads them, uses them for the SSH session, and never prints,
  writes or commits them.
- The server fetches from the public `origin` remote directly; no
  credentials are shipped to or stored on the server by this flow.
- Nothing about the server (hostname, user) belongs in commits, logs or
  PR descriptions.

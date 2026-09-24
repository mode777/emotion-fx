# Tasks

Notes on the project rules for this change: no durable architecture
decision is settled (design D3), so there is no ADR task; no invariant,
`docs/js-api.md`, or AGENTS.md current-state text changes, so there is no
docs task. The four-target gate (ctest smoke + golden harness) is not this
change's verification harness — no code it builds changes; the harness here
is the Pages deploy itself plus live-site checks (design D2).

## 1. Workflow fix

- [x] 1.1 In `.github/workflows/pages.yml`, extend the "Collect pages content" step to check every expected output file exists before copying (`test -f` loop over `player_web.html`, `player_web.js`, `player_web.wasm`, `player_web.data`, mirroring the `ci.yml` web-bundle step) and add `build-web/player_web.data` to the copy list. Verify: `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/pages.yml'))"` parses, and the step text lists all four files.
- [x] 1.2 Confirm the guarded list matches the build's real output: on the SSH verification server (emsdk 3.1.64, per `docs/verification-server.md`) run `emcmake cmake -B build-web-fix -DCMAKE_BUILD_TYPE=Release && cmake --build build-web-fix --target player_web` and confirm exactly the four files exist in `build-web-fix/`. Verify: `ls build-web-fix/player_web.{html,js,wasm,data}` succeeds — no fifth expected file, none missing.

## 2. Deploy and live-site verification

- [ ] 2.1 Branch, commit the one-step workflow change, and merge to `main` (Pages deploys only from the default branch — design Context). Verify: the push starts a `pages` workflow run (`gh run list --workflow=pages.yml`).
- [ ] 2.2 Watch the `pages` run to completion and confirm it is green, including the existence-check step. Verify: `gh run view <run-id>` shows the deploy job succeeded; if the existence check fails, fix the file list in the same branch and re-merge.
- [ ] 2.3 Verify the deployed file set over HTTP with a cache-busting query: `curl -s -o /dev/null -w "%{http_code} %\\n" "https://mode777.github.io/emotion-fx/player_web.data?cb=$(date +%s)"` for `.data` (the regression), plus `.js`, `.wasm`, and the page itself. Verify: all return 200 with non-zero size — `.data` was 404 before the fix.
- [ ] 2.4 Verify the demo actually boots: load <https://mode777.github.io/emotion-fx/> in a browser with the console open. Verify: no `player_web.data` 404, no `Uncaught Error` from the Emscripten loader (`xhr.onload`), and the demo window renders and animates.

## 3. Wrap-up

- [ ] 3.1 Confirm the `verification` spec delta ("Deployed site serves the complete web player", "Missing output file fails the deploy") matches the shipped behavior, then mark tasks complete and close the change via the archive workflow. Verify: `npx openspec validate --strict` passes for this change before archiving.

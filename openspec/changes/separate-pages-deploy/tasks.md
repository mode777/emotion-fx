# Tasks

## 1. New Pages workflow

- [x] 1.1 Create `.github/workflows/pages.yml` with `name: pages`, triggers `push: branches: [main]` and `workflow_dispatch`, and a deploy job gated `if: github.ref == 'refs/heads/main'` carrying `permissions: {contents: read, pages: write, id-token: write}` and `environment: {name: github-pages, url: ...}`; move the existing `pages` steps in (checkout, `setup-emsdk@v14` pinned to `3.1.64`, build the `player_web` target, collect `pages/index.html` + `.js` + `.wasm`, `upload-pages-artifact`, `deploy-pages`); verify the YAML parses (actionlint, or `python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/pages.yml'))"`) and the job condition references `refs/heads/main`
- [x] 1.2 Confirm the pinned Emscripten version and the built file set match the old job exactly (`player_web.html` → `index.html`, plus `player_web.js` and `player_web.wasm`) so the deployed site is unchanged; verify by diffing the moved steps against the removed job

## 2. Remove Pages from the gate workflow

- [x] 2.1 Delete the `pages` job from `.github/workflows/ci.yml`; verify the remaining jobs are exactly `native`, `emscripten`, `golden-web`, `generate-goldens`, `release` and that the file still parses (`python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/ci.yml'))"` or actionlint)
- [x] 2.2 Grep the repo for remaining references to the old `pages` job (e.g. `rg -n "deploy web player|deploy-pages" .github docs AGENTS.md`) and confirm the only Pages references left are in `pages.yml` and the amended ADR

## 3. Docs and ADR

- [x] 3.1 Amend `docs/decisions/0023-tag-triggered-ci-and-releases.md`: add an amendment note to Status, rewrite the Pages clause to "a separate workflow deploying on default-branch push or manual dispatch", move "moving Pages to its own workflow" out of Rejected alternatives (recording why the earlier calculus changed), and confirm the `docs/decisions/README.md` index row still describes the record
- [x] 3.2 Add a short current-state line to the `AGENTS.md` CI section noting Pages deploys through a separate `pages.yml` workflow on `main` (not the gate), so the CI description stays complete; verify the manual-run guidance ("Use a manual run to prove the gate") remains accurate

## 4. Verification

- [x] 4.1 Run `npx openspec validate --change separate-pages-deploy --strict` and confirm the change passes
- [ ] 4.2 Commit and push the change on a branch and run a manual gate dispatch (`gh workflow run ci.yml --ref <branch>`); confirm the run is green with no `pages` job — the acceptance criterion this change exists for (previously this ref failed on `pages`)
- [ ] 4.3 After the change reaches `main`, confirm the Pages workflow is dispatchable and deploys: `gh workflow view pages.yml`, then `gh workflow run pages.yml --ref main` and check the run succeeds and the Pages URL serves the built web player; also confirm a manual `pages.yml` dispatch from a feature branch skips the deploy job (no protected-environment failure)

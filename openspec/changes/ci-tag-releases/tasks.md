# Tasks

Implements the tag/manual CI triggers and downloadable release artifacts.
Decision record: `docs/decisions/0023-tag-triggered-ci-and-releases.md`
(written during apply). Behavior contract: the `verification` delta.
Design: `design.md`.

## 1. Trigger policy

- [x] 1.1 Replace the workflow `on:` block with `push.tags: ['v*']` and `workflow_dispatch`, removing `push.branches: [main]` and `pull_request`; verify the YAML parses (`gh workflow view ci.yml` or an actionlint run) and the only remaining triggers are a `v*` tag and manual dispatch
- [x] 1.2 Re-gate the `pages` job to tag-or-manual (`startsWith(github.ref, 'refs/tags/v') || github.event_name == 'workflow_dispatch'`) so Pages deploys on releases, and confirm `generate-goldens` stays `workflow_dispatch`-only; verify by inspecting the rendered conditions

## 2. Per-target packaging

- [x] 2.1 Read the Emscripten player bundle's actual output filenames from the CMake target/build dir, add a packaging step that zips the HTML loader + JS glue + wasm + data, and `actions/upload-artifact` it from the `emscripten` job; verify the archive lists all four files and no partial bundle is possible (step fails on a missing file)
- [ ] 2.2 Add per-OS packaging to the `native` matrix (tar on Linux/macOS, zip on Windows) producing `emotion-fx-<version>-<os>-<arch>` and upload each with `actions/upload-artifact`; verify a local matrix run or a workflow run yields one native archive per OS

## 3. Release attachment

- [x] 3.1 Add a `release` job (`needs: [native, emscripten]`, `if: startsWith(github.ref, 'refs/tags/v')`, `permissions: contents: write`) that downloads all run artifacts and runs `gh release create "$GITHUB_REF_NAME" --generate-notes` plus asset uploads; verify it uses no new third-party action and is the only job with write permission
- [ ] 3.2 Confirm all four target archives are attached and the development-version naming (`dev-<sha>`) is used for manual runs while tag runs use the tag name; verify the naming step output on both event paths

## 4. Documentation and ADR

- [x] 4.1 Write `docs/decisions/0023-tag-triggered-ci-and-releases.md` per `TEMPLATE.md` (context, decision, consequences, rejected alternatives) and add its row to `docs/decisions/README.md`
- [x] 4.2 Mark ADR 0009's trigger paragraph superseded by 0023 (status line + README index row) without deleting the record
- [x] 4.3 Update the CI wording in `AGENTS.md` and `README.md` to state the tag/manual trigger policy, `gh workflow run ci.yml`, and the per-run downloadable archives

## 5. Verification

- [ ] 5.1 Run the full gate via `gh workflow run ci.yml` on a ref and confirm every job (native matrix, emscripten, golden-web) passes the existing ctest smoke suite and golden-image harness on all four targets, and that four archives are downloadable from the run
- [ ] 5.2 Push a throwaway `v*` tag and confirm the release job creates a GitHub Release whose assets are the four target archives, then delete the test release and tag
- [ ] 5.3 Verify a normal branch push and a PR open do not start the workflow (no run appears), confirming the resource-saving trigger policy

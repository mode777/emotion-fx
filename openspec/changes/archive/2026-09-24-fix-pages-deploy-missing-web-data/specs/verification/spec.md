# Spec Delta: verification

## MODIFIED Requirements

### Requirement: Separate Pages deployment workflow
The public web player's GitHub Pages build and deployment SHALL be a
separate workflow (`.github/workflows/pages.yml`), not a job inside the gate
workflow. That workflow SHALL run on pushes to the default branch and on
`workflow_dispatch`, and its deployment job SHALL be gated to the default
branch so a manual dispatch from another ref is a no-op rather than a
protected-environment failure. It SHALL build the same Emscripten web-player
target the gate builds and deploy it with the GitHub Pages actions, and its
Emscripten toolchain SHALL be pinned consistently with the rest of CI
(ADR 0020). The deployed content SHALL include the complete output file set
of that target — HTML loader, JS glue, wasm module, and the preloaded data
file the JS glue fetches at startup (currently `player_web.html`,
`player_web.js`, `player_web.wasm`, and `player_web.data`) — so the deployed
demo loads without missing-resource errors. The workflow SHALL check that
every expected output file exists before publishing and SHALL fail when one
is absent rather than deploy an incomplete set.

#### Scenario: Default-branch push deploys the web player
- **WHEN** a commit is pushed to the default branch
- **THEN** the Pages workflow builds the web player and deploys it to
  GitHub Pages

#### Scenario: Manual dispatch from the default branch deploys
- **WHEN** a user manually dispatches the Pages workflow from the default
  branch
- **THEN** the web player is built and deployed to GitHub Pages

#### Scenario: Manual dispatch from another ref is a no-op
- **WHEN** the Pages workflow is manually dispatched from a non-default ref
- **THEN** the deployment job is skipped (no protected-environment failure)
  and the workflow does not deploy

#### Scenario: Pages deployment is not part of the gate
- **WHEN** a gate run completes, whether triggered by a tag or manually
- **THEN** the gate workflow contains no Pages deployment job, so the run's
  status reflects only the verification (and tag-release) jobs

#### Scenario: Deployed site serves the complete web player
- **WHEN** the Pages workflow deploys from the default branch
- **THEN** every file of the web player's output set is reachable at the
  deployed URL — in particular the preloaded data file returns 200, not
  404 — and the demo starts without resource-load errors

#### Scenario: Missing output file fails the deploy
- **WHEN** a file expected by the Pages collect step is absent from the
  build output
- **THEN** the collect step fails and the workflow publishes no deployment,
  instead of silently deploying an incomplete web player

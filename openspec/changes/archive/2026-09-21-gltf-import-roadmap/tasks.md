# Tasks

## 1. Roadmap spec delta application

- [x] 1.1 Apply the delta to `openspec/specs/feature-roadmap/spec.md`: "Fixed milestone order" — F6 scope becomes "resource packaging + glTF 2.0 asset import + REPL" with the import payload named (meshes, images, skins, animation clips); "Early risk retirement" — add the sentence pinning the glTF 2.0 format at roadmap level and deferring only the profile (container, extensions, image embedding) to F6; add "Third-party dependency evaluation at proposal time" with its three scenarios; verify with `npx openspec validate --strict`

- [x] 1.2 No ADR in this change — confirm the decision stands (glTF data model is ADR 0014; profile deferral is roadmap material per design D1/D2) and that `docs/decisions/README.md` needs no new row; verify by inspection

## 2. AGENTS.md syncs

- [x] 2.1 Update the roadmap table F6 row to "Zip resource root, glTF 2.0 asset import (profile decided here), interactive REPL" and the Deferred-decisions paragraph from "the asset format (F6)" to "the glTF import profile (F6)"; verify the table and paragraph match the applied spec text

- [x] 2.2 Add the OpenSpec invocation note: CLI is not on PATH — run `npm install` once, then `npx openspec …` from the repo root; verify the note renders and the command form works (`npx openspec list`)

## 3. js-api.md sync

- [x] 3.1 Update the "Asset format" open question and the F6 section's "signatures final once the asset format is decided" note: format is pinned (glTF 2.0, meshes/images/skins/clips); the F6 change settles only the profile; `load*` signatures stay provisional until then; verify the traceability section still holds and no catalog entry changed

## 4. Verification

- [x] 4.1 Docs-only change: the ctest smoke suite is not exercised (no code touched); run `npx openspec validate --strict` and cross-check that roadmap spec, AGENTS.md (table + deferred list), and `docs/js-api.md` state the same thing about the glTF format and profile

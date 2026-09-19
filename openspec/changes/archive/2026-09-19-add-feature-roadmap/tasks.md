# Tasks

## 1. Document the roadmap in AGENTS.md

- [x] 1.1 Add a "Roadmap" section to `AGENTS.md` containing the F1–F8 ladder (name + one-line scope per milestone), the fixed-order rule, and each milestone's verification gate; reference `openspec/specs/feature-roadmap` for the normative version. Verify: reading `AGENTS.md` alone lets a session state the milestone order, per-milestone scope, and gates.
- [x] 1.2 Mark milestone status in the `AGENTS.md` section (F1–F8 all open/planned; none started). Verify: the status list exists and matches the roadmap section content.

## 2. Consistency and validation

- [x] 2.1 Check the `AGENTS.md` section against `specs/feature-roadmap/spec.md` in this change: identical milestone list and order (F1 skeleton, F2 2D+display list+harness, F3 3D core, F4a/F4b lighting+Phong, F5 RTT+post FX, F6 packaging+import+REPL, F7 skinning+animation, F8 high-level JS+text+demo) and identical gate definitions. Verify: a line-by-line cross-check finds no contradiction.
- [x] 2.2 Run `npx openspec validate add-feature-roadmap --strict` and confirm it passes; confirm all four artifacts (proposal, specs, design, tasks) report done via `npx openspec status --change add-feature-roadmap`. Verify: validate exits zero, status lists no pending required artifacts.

# Spec Delta

## Purpose

Defines the verification harness for milestone gates: scripted smoke tests
that cross the JS/C boundary and assert exit codes, which must pass on all four
targets. This is the F1 gate and the foundation that the F2 golden-image
harness will extend.

## ADDED Requirements

### Requirement: Smoke-test suite
The repository SHALL contain a smoke-test suite consisting of scripted tests
run via the player's `--script` mode. Each test SHALL assert its outcome via
exit code and SHALL exercise the JS/C boundary (calling engine-exposed
functions from JS).

#### Scenario: Suite run on a built target
- **WHEN** the smoke-test suite is run against a player build on any supported
  target
- **THEN** every test completes with exit code 0

### Requirement: Four-target gate
The full smoke-test suite MUST pass on Windows, Linux, macOS, and Emscripten
before the next roadmap milestone starts.

#### Scenario: Gate evaluation for F1
- **WHEN** the F1 gate is evaluated
- **THEN** the build matrix succeeds on all four targets and the smoke suite
  passes on all four targets with zero failures

### Requirement: Test failure reporting
A failing smoke test SHALL produce a non-zero exit status and a diagnostic that
identifies the failing test by name.

#### Scenario: Identifying a failure
- **WHEN** a smoke test fails on any target
- **THEN** the suite reports the failing test's name and exits non-zero

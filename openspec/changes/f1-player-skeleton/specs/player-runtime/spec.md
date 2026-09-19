# Spec Delta

## Purpose

Defines the player executable's observable behavior: loading a resource root,
picking up the `main.js` entry script and its lifecycle hooks, the window and
frame loop, the `--script` run mode, and the exit-code contract.

## ADDED Requirements

### Requirement: Resource root loading
Given a path to a resource root directory, the player SHALL treat that
directory as the root of all game resources and SHALL load `main.js` from it as
the entry script. Code at the top level of `main.js` SHALL run once at load
time, before the frame loop starts.

#### Scenario: Valid resource root
- **WHEN** the player is launched with a directory containing `main.js`
- **THEN** the entry script is loaded and executed

#### Scenario: Missing entry script
- **WHEN** the player is launched with a directory that does not contain
  `main.js`
- **THEN** the player prints a diagnostic and exits with a non-zero code

### Requirement: Lifecycle hooks
The player SHALL invoke the `update` and `render` functions defined by the
entry script once per frame, `update` before `render`. A hook that is not
defined by the script SHALL be skipped without error.

#### Scenario: Hooks invoked per frame
- **WHEN** the entry script defines `update` and `render`
- **THEN** each defined hook is called once per frame in that order

#### Scenario: Absent hook tolerated
- **WHEN** the entry script defines only `update`
- **THEN** the player runs the frame loop calling `update` only, without error

### Requirement: Window and frame loop
In resource-root mode the player SHALL open a platform window with a
renderer-defined clear color and SHALL run the frame loop until the window is
closed or the script requests termination.

#### Scenario: Window closed by user
- **WHEN** the user closes the window during the frame loop
- **THEN** the player shuts down cleanly and exits with code 0

#### Scenario: Script requests termination
- **WHEN** the entry script calls the engine's quit function with an exit code
- **THEN** the player stops the frame loop and exits with that code

### Requirement: Script run mode
The player SHALL support a `--script <path>` run mode that executes a single
script file without opening a window and exits as soon as the script finishes.
This mode is the vehicle for automated smoke tests.

#### Scenario: Headless script execution
- **WHEN** the player is launched with `--script` and a valid script path
- **THEN** no window is opened, the script runs to completion, and the player
  exits

### Requirement: Exit-code contract
The player SHALL exit 0 when the run completes successfully and a non-zero code
when it fails. Failure cases MUST include: missing resource root or entry
script, script file not found in `--script` mode, and an uncaught script
error. Diagnostics for failures SHALL be printed to stderr.

#### Scenario: Script exit code propagation
- **WHEN** a `--script` run ends by requesting exit code 3
- **THEN** the player process exits with code 3

#### Scenario: Script not found
- **WHEN** the player is launched with `--script` and a path that does not exist
- **THEN** the player prints a diagnostic to stderr and exits with a non-zero
  code

#### Scenario: Uncaught script error
- **WHEN** a script run ends with an uncaught script exception
- **THEN** the player prints the error to stderr and exits with a non-zero code

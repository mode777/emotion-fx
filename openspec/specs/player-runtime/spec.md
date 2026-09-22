# player-runtime

## Purpose

Defines the player executable's observable behavior: loading a resource root,
picking up the `main.js` entry script and its lifecycle hooks, the window and
frame loop, the `--script` run mode, and the exit-code contract.

## Requirements

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
The player SHALL drive per-frame callbacks through explicit hook registration.
`efx.registerUpdateHook(fn)` and `efx.registerRenderHook(fn)` SHALL stack hooks
in registration order; each frame SHALL run all update hooks, each receiving
`dt` (seconds since the previous frame), before all render hooks, and each
registration SHALL return an unsubscribe function that removes it and is
idempotent. The global `update` and `render` functions defined by the entry
script SHALL be registered as load-time sugar in load order once the script
finishes evaluating, after any hooks registered during evaluation; a script
that defines neither global hooks nor registrations SHALL run the frame loop
without error. An uncaught exception in any hook SHALL stop the frame loop and
exit with a non-zero code.

#### Scenario: Hooks invoked per frame
- **WHEN** the entry script registers update and render hooks (or defines
  global `update`/`render`)
- **THEN** all update hooks run before all render hooks once per frame, each
  update hook receiving `dt`

#### Scenario: Absent hook tolerated
- **WHEN** the entry script defines neither `update`/`render` nor any
  registration
- **THEN** the player runs the frame loop without error

#### Scenario: Unsubscribe removes a hook
- **WHEN** a script calls the unsubscribe function returned by a registration
- **THEN** that hook no longer runs on later frames, and calling unsubscribe
  again has no effect

#### Scenario: Global hooks remain load-time sugar
- **WHEN** the entry script defines global `update`/`render` and registers no
  explicit hooks
- **THEN** the player registers them after evaluation in load order, so F1
  scripts keep working unchanged

#### Scenario: Hook exception stops the loop
- **WHEN** any registered hook throws an uncaught exception
- **THEN** the frame loop stops, the error is printed, and the player exits
  non-zero

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

### Requirement: Web entry script and native lifecycle
On Emscripten, in resource-root mode, the player SHALL load `main.js` from
the resource root, execute it with the browser's native JS engine, and
wire the same global `update`/`render` lifecycle hooks (and explicit hook
registration) once per frame, `update` before `render`, matching the
desktop contract. An uncaught error in the entry script or a hook SHALL
stop the frame loop and surface through the same non-zero exit-code
contract (message on the console/error channel). A missing `main.js`
SHALL produce a diagnostic and the error exit code.

#### Scenario: Browser executes the entry script
- **WHEN** the web player is launched with a resource root whose `main.js`
  defines `update` and `render`
- **THEN** the browser engine executes the entry script once before the
  frame loop and calls the hooks once per rendered frame in order

#### Scenario: Web hook error surfaces
- **WHEN** the entry script or a hook throws an uncaught exception
- **THEN** the frame loop stops, the error is reported on the error
  channel, and the run ends with the failure exit code

#### Scenario: Missing entry script on web
- **WHEN** the web player is launched with a resource root without
  `main.js`
- **THEN** a diagnostic is reported and the run ends with a non-zero exit
  code

# Spec Delta

## Purpose

Defines the embedded ES6 script runtime and the JavaScript/C boundary: script
execution with a bundled interpreter, the mechanism by which engine functions
implemented in C are callable from JS, the dependency restrictions on scripts,
and error propagation.

## ADDED Requirements

### Requirement: Bundled ES6 execution
The player MUST execute scripts with its own bundled ES6 interpreter on all
platforms: the same interpreter binary is embedded on desktop platforms, and on
Emscripten the page's JS engine drives the compiled core through a bridge with
equivalent behavior. Game scripts MUST NOT depend on browser or Node.js APIs,
neither directly nor transitively.

#### Scenario: Portable script runs identically everywhere
- **WHEN** a smoke script that uses only standard ES6 features and
  engine-exposed functions is run on all four targets
- **THEN** it produces the same observable output and exit code on each

#### Scenario: Engine-provided global namespace
- **WHEN** any script starts
- **THEN** the engine functions are available in a single well-known global
  namespace without any import or setup by the script

### Requirement: JS-to-C function calls
The engine SHALL expose its C-implemented functions to scripts through the
global namespace, such that a script call crosses into C, executes, and returns
a result or effect to the script. F1 SHALL expose at least: a log/print
function, a script-requested quit function taking an exit code, and a means to
read script arguments provided by the host.

#### Scenario: Log crossing
- **WHEN** a script calls the engine log function with a string
- **THEN** the string appears on the player's stdout

#### Scenario: Quit crossing
- **WHEN** a script calls the engine quit function with code 3
- **THEN** the C core terminates the run with exit code 3

#### Scenario: Host arguments readable
- **WHEN** the host launches a `--script` run with extra arguments
- **THEN** the script can read those arguments through the engine API

### Requirement: Error propagation to exit codes
An uncaught script exception MUST surface as a non-zero player exit code with
the error message on stderr, in both `--script` mode and the frame-loop hooks.
A stack or error trace SHALL be included in the diagnostic output.

#### Scenario: Exception in script mode
- **WHEN** a `--script` run throws an uncaught exception
- **THEN** the player exits non-zero and stderr contains the error message

#### Scenario: Exception in a frame hook
- **WHEN** `update` or `render` throws an uncaught exception during the frame
  loop
- **THEN** the player stops the loop, prints the error to stderr, and exits
  non-zero

# Spec Delta

## ADDED Requirements

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

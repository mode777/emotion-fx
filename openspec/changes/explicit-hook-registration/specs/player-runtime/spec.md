# Spec Delta: player-runtime

## MODIFIED Requirements

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

// Explicit lifecycle hooks (ADR 0016). `examples/hello` uses the global
// `update`/`render` functions, which remain supported as load-time sugar;
// this sample registers hooks explicitly and keeps the unsubscribe function.
let frames = 0;

const offUpdate = efx.registerUpdateHook(function (dt) {
    frames++;
    if (frames === 1) {
        efx.log('hooks: first update, dt=' + dt);
    }
    if (frames >= 60) {
        offUpdate(); // stop receiving updates before quitting
        efx.log('hooks: 60 frames reached, quitting');
        efx.quit(0);
    }
});

efx.registerRenderHook(function () {
    // Record draw calls here (see examples/browser for the F2 gallery).
});

// explicit lifecycle hooks (ADR 0016): stacking order, self-unsubscribe
// during dispatch, dt on update hooks, and quit from a render hook.
let updates = 0;
let frames = 0;

const offFirst = efx.registerUpdateHook(function (dt) {
    if (typeof dt !== 'number' || !isFinite(dt)) {
        efx.log('bad-dt');
        efx.quit(1);
        return;
    }
    updates++;
    efx.log('u' + updates);
    if (updates === 1) {
        offFirst(); // self-unsubscribe while running
    }
});

efx.registerUpdateHook(function () {
    efx.log('u2');
});

efx.registerRenderHook(function () {
    frames++;
    efx.log('r' + frames);
});

efx.registerRenderHook(function () {
    if (frames >= 2) {
        efx.log('done');
        efx.quit(0);
    }
});

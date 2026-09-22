let n = 0;

const offA = efx.registerUpdateHook(function () {
    efx.log('a');
});

efx.registerUpdateHook(function () {
    n++;
    if (n === 1) {
        offA();
        efx.log('unsub');
    }
    if (n === 2) {
        efx.quit(0);
    }
});

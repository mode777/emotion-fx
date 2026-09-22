let n = 0;

efx.registerUpdateHook(function (dt) {
    n++;
    if (n === 1) efx.log('eu1');
    if (n === 3) { efx.log('eu3-quit'); efx.quit(5); }
});

efx.registerRenderHook(function () {
    if (n === 1) efx.log('er1');
});

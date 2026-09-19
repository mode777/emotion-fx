const a = efx.args();
if (a.length !== 2 || a[0] !== 'one' || a[1] !== 'two') {
    efx.log('args mismatch: ' + JSON.stringify(a));
    efx.quit(1);
}

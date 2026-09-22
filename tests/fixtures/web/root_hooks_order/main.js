let n = 0;

function update() {
    n++;
    if (n === 1) efx.log('u1');
    if (n === 2) { efx.log('u2'); efx.quit(0); }
}

function render() {
    efx.log('r' + n);
}

let n = 0;
function update() {
    n++;
    if (n === 1) efx.log('u1');
    if (n === 3) { efx.log('u3-quit'); efx.quit(5); }
}
function render() {
    if (n === 1) efx.log('r1');
}

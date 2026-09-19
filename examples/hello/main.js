let frames = 0;

function update() {
    frames++;
    if (frames === 1) {
        efx.log('hello: first update');
    }
    if (frames >= 60) {
        efx.log('hello: 60 frames reached, quitting');
        efx.quit(0);
    }
}

function render() {
    if (frames === 1) {
        efx.log('hello: first render');
    }
}

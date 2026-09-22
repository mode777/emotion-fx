let frames = 0;

function update() {
    frames++;
    efx.log('frame-' + frames);
    if (frames >= 1) {
        efx.quit(0);
    }
}

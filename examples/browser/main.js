let frames = 0;
let lastLogged = 0;

function update() {
    frames++;
    if (frames - lastLogged >= 60) {
        lastLogged = frames;
        efx.log('browser hello: frame ' + frames);
    }
}

function render() {
    if (frames === 1) {
        efx.log('browser hello: first render (clear color visible)');
    }
}

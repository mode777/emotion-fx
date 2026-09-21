efx.setClearColor([0.05, 0.05, 0.08, 1]);
function update() {}
function render() {
    efx.drawQuad(220, 150, 200, 180, efx.whiteTexture, {
        color: [0.9, 0.2, 0.1, 1],
        rotation: 30,
        scale: 1.2,
    });
}

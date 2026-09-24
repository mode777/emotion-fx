efx.setClearColor([0.02, 0.06, 0.1, 1]);
function update() {}
function render() {
    // both quads rotate/scale around their own centers (320, 240)
    efx.drawQuad(270, 230, efx.whiteTexture, {
        size: [100, 20],
        color: [1, 1, 1, 1], rotation: 45, scale: 2.5,
    });
    efx.drawQuad(310, 190, efx.whiteTexture, {
        size: [20, 100],
        color: [1, 0.7, 0.1, 1], rotation: 45, scale: 2.5,
    });
}

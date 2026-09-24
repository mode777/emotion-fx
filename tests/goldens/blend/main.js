efx.setClearColor([0, 0, 0, 1]);
const tex = efx.createTexture(
    efx.createImageData({ width: 2, height: 2, pixels: [200, 200, 200, 255, 200, 200, 200, 255, 200, 200, 200, 255, 200, 200, 200, 255] }));
function update() {}
function render() {
    efx.setBlendMode('additive');
    efx.drawQuad(140, 140, tex, { size: [200, 200], color: [0.15, 0.35, 0.15, 1] });
    efx.setBlendMode('alpha');
    efx.drawQuad(260, 180, tex, { size: [200, 200], color: [1, 1, 1, 0.5] });
    efx.setBlendMode('subtractive');
    efx.drawQuad(340, 220, tex, { size: [180, 180], color: [0.4, 0.1, 0.4, 1] });
    efx.setBlendMode('alpha');
}

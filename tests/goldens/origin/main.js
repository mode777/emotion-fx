efx.setClearColor([0.02, 0.06, 0.1, 1]);
// 16x8 texture: left half orange, right half teal
const pixels = [];
for (let y = 0; y < 8; y++) {
    for (let x = 0; x < 16; x++) {
        if (x < 8) {
            pixels.push(230, 140, 30, 255);
        } else {
            pixels.push(30, 160, 170, 255);
        }
    }
}
const tex = efx.createTexture(
    efx.createImageData({ width: 16, height: 8, pixels: pixels }));
function update() {}
function render() {
    // top-left pair: size derivation — 1:1 texture-size draw (16x8) and a
    // sourceRect-derived size (left half -> 8x8), both untransformed so
    // origin defaults are invisible
    efx.drawQuad(24, 20, tex);
    efx.drawQuad(60, 20, tex, { sourceRect: { x: 0, y: 0, w: 8, h: 8 } });

    // middle: scale applies after the size is determined — 64x16 scaled 3x
    // around its center covers 192x48 centered on (256, 224)
    efx.drawQuad(224, 216, tex, { size: [64, 16], scale: 3 });

    // bottom-left: origin moves the pivot — rotation 90 pivots on the
    // quad-local point (4, 4), which stays fixed at (100, 344); the quad
    // swings right and down instead of spinning in place
    efx.drawQuad(96, 340, tex, { size: [128, 64], origin: [4, 4], rotation: 90 });

    // bottom-right: corner pivot with derived size, rotation + scale combined
    efx.drawQuad(460, 320, tex, { origin: [0, 0], rotation: 45, scale: 1.5 });
}

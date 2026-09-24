// F2 smoke: argument validation of the 2D API (headless — throws must be
// standard ES6 errors; draw calls that need a GPU surface are covered by
// windowed/golden tests)
function expectThrow(name, fn) {
    try { fn(); efx.log('FAIL no-throw ' + name); efx.quit(1); }
    catch (e) {
        if (!(e instanceof TypeError) && !(e instanceof RangeError)) {
            efx.log('FAIL type ' + name + ': ' + e); efx.quit(2);
        }
    }
}
expectThrow('clearColor-wrongtype', () => efx.setClearColor('red'));
expectThrow('clearColor-short', () => efx.setClearColor([1, 2, 3]));
expectThrow('camera-noargs', () => efx.setCamera2D());
expectThrow('camera-zoom', () => efx.setCamera2D({ x: 0, y: 0, zoom: 0 }));
expectThrow('camera-frame', () => efx.setCamera2D({ frame: [0, 0] }));
expectThrow('camera-frame3', () => efx.setCamera2D({ frame: [10, 10, 10] }));
expectThrow('blend-unknown', () => efx.setBlendMode('nope'));
expectThrow('blend-missing', () => efx.setBlendMode());
expectThrow('img-short', () => efx.createImageData({ width: 2, height: 2, pixels: [1, 2, 3] }));
expectThrow('img-fmt', () => efx.createImageData({ width: 1, height: 1, pixels: [0, 0, 0, 0], format: 'bgr' }));
expectThrow('img-unknown', () => efx.createImageData({ width: 1, height: 1, pixels: [0, 0, 0, 0], pixles: 1 }));
expectThrow('img-size', () => efx.createImageData({ width: 0, height: 8, pixels: [] }));

// drawQuad(x, y, texture, opts?): texture required live; size/origin
// validation; a created texture works headless (uploads queue until a
// GPU surface exists), efx.whiteTexture needs a window and is covered by
// the unit/golden suites
const img = efx.createImageData({ width: 8, height: 4, pixels: new Uint8Array(8 * 4 * 4) });
const tex = efx.createTexture(img);
if (tex.width !== 8 || tex.height !== 4) { efx.log('FAIL texture size getters'); efx.quit(3); }
expectThrow('quad-few', () => efx.drawQuad(0, 0));
expectThrow('quad-nontexture', () => efx.drawQuad(0, 0, {}));
expectThrow('quad-size-zero', () => efx.drawQuad(0, 0, tex, { size: [0, 10] }));
expectThrow('quad-size-short', () => efx.drawQuad(0, 0, tex, { size: [10] }));
expectThrow('quad-size-type', () => efx.drawQuad(0, 0, tex, { size: 'big' }));
expectThrow('quad-size-nan', () => efx.drawQuad(0, 0, tex, { size: [NaN, 1] }));
expectThrow('quad-origin-nan', () => efx.drawQuad(0, 0, tex, { origin: [NaN, 0] }));
expectThrow('quad-origin-type', () => efx.drawQuad(0, 0, tex, { origin: 'center' }));
expectThrow('quad-src-zero', () => efx.drawQuad(0, 0, tex, { sourceRect: { x: 0, y: 0, w: 0, h: 2 } }));
expectThrow('quad-src-oob', () => efx.drawQuad(0, 0, tex, { sourceRect: { x: 0, y: 0, w: 9, h: 2 } }));
expectThrow('quad-unknown-opt', () => efx.drawQuad(0, 0, tex, { colour: [1, 1, 1, 1] }));
tex.destroy();
expectThrow('quad-destroyed-texture', () => efx.drawQuad(0, 0, tex));
expectThrow('getter-destroyed-texture', () => tex.width);
expectThrow('getter-destroyed-texture-h', () => tex.height);
efx.log('2d-validation-ok');
efx.quit(0);

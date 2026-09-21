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
expectThrow('quad-few', () => efx.drawQuad(0, 0, 4, 4));
expectThrow('quad-negative', () => efx.drawQuad(0, 0, -4, 4, null));
expectThrow('quad-nontexture', () => efx.drawQuad(0, 0, 4, 4, {}));
efx.log('2d-validation-ok');
efx.quit(0);

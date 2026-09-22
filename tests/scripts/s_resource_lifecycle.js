// F2 resource lifecycle smoke: destroy() is deterministic and idempotent,
// using a destroyed resource throws, and a texture cannot be created from a
// destroyed ImageData. Runs headless on both runtimes (no GPU surface:
// texture uploads queue in C until a sink exists).
function expectThrow(name, fn) {
    try { fn(); efx.log('FAIL no-throw ' + name); efx.quit(1); }
    catch (e) {
        if (!(e instanceof TypeError)) {
            efx.log('FAIL type ' + name + ': ' + e); efx.quit(2);
        }
    }
}

const img = efx.createImageData({ width: 2, height: 2, pixels: new Uint8Array(16) });
const tex = efx.createTexture(img);

tex.destroy();
tex.destroy();
expectThrow('draw-destroyed-texture', () => efx.drawQuad(0, 0, 4, 4, tex));

img.destroy();
img.destroy();
expectThrow('texture-from-destroyed-image', () => efx.createTexture(img));

efx.log('resource-lifecycle-ok');
efx.quit(0);

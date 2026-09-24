// Golden gallery (f2c visual demo): cycles the seven golden-image scenes
// through the public F2 API. Each scene reproduces its committed capture
// under tests/goldens/<name>/golden.png on a 640x480 virtual frame; a
// progress strip at the bottom shows the cycle position.
const FRAMES_PER_SCENE = 150;
const FRAME = [640, 480];

const scenes = [
    {
        name: 'clear',
        setup() {
            efx.setClearColor([0.1, 0.7, 0.3, 1]);
            efx.setCamera2D({ frame: FRAME });
        },
        render() {},
    },
    {
        name: 'solid',
        setup() {
            efx.setClearColor([0.05, 0.05, 0.08, 1]);
            efx.setCamera2D({ frame: FRAME });
        },
        render() {
            efx.drawQuad(220, 150, efx.whiteTexture, {
                size: [200, 180],
                color: [0.9, 0.2, 0.1, 1],
                rotation: 30,
                scale: 1.2,
            });
        },
    },
    {
        name: 'srcrect',
        setup() {
            efx.setClearColor([0.08, 0.09, 0.12, 1]);
            efx.setCamera2D({ frame: FRAME });
            const pixels = [];
            for (let y = 0; y < 4; y++) {
                for (let x = 0; x < 4; x++) {
                    const on = (x + y) % 2 === 0;
                    pixels.push(on ? 255 : 30, 40, on ? 30 : 200, 255);
                }
            }
            this.tex = efx.createTexture(
                efx.createImageData({ width: 4, height: 4, pixels: pixels }));
        },
        render() {
            efx.drawQuad(60, 60, this.tex, { size: [160, 160] });
            efx.drawQuad(280, 60, this.tex, {
                size: [160, 160],
                sourceRect: { x: 0, y: 0, w: 2, h: 4 },
            });
            efx.drawQuad(500, 60, this.tex, {
                size: [80, 160],
                sourceRect: { x: 0, y: 0, w: 1, h: 1 },
            });
        },
    },
    {
        name: 'pivot',
        setup() {
            efx.setClearColor([0.02, 0.06, 0.1, 1]);
            efx.setCamera2D({ frame: FRAME });
        },
        render() {
            efx.drawQuad(270, 230, efx.whiteTexture, {
                size: [100, 20],
                color: [1, 1, 1, 1], rotation: 45, scale: 2.5,
            });
            efx.drawQuad(310, 190, efx.whiteTexture, {
                size: [20, 100],
                color: [1, 0.7, 0.1, 1], rotation: 45, scale: 2.5,
            });
        },
    },
    {
        name: 'blend',
        setup() {
            efx.setClearColor([0, 0, 0, 1]);
            efx.setCamera2D({ frame: FRAME });
            this.tex = efx.createTexture(efx.createImageData({
                width: 2, height: 2,
                pixels: [200, 200, 200, 255, 200, 200, 200, 255,
                    200, 200, 200, 255, 200, 200, 200, 255],
            }));
        },
        render() {
            efx.setBlendMode('additive');
            efx.drawQuad(140, 140, this.tex, { size: [200, 200], color: [0.15, 0.35, 0.15, 1] });
            efx.setBlendMode('alpha');
            efx.drawQuad(260, 180, this.tex, { size: [200, 200], color: [1, 1, 1, 0.5] });
            efx.setBlendMode('subtractive');
            efx.drawQuad(340, 220, this.tex, { size: [180, 180], color: [0.4, 0.1, 0.4, 1] });
            efx.setBlendMode('alpha');
        },
    },
    {
        name: 'camera',
        setup() {
            efx.setClearColor([0.05, 0.05, 0.1, 1]);
            efx.setCamera2D({ frame: FRAME, x: 320, y: 240, zoom: 1.5, rotation: 20 });
        },
        render() {
            for (let gy = 0; gy < 3; gy++) {
                for (let gx = 0; gx < 3; gx++) {
                    efx.drawQuad(200 + gx * 100, 140 + gy * 80, efx.whiteTexture, {
                        size: [60, 40],
                        color: [0.2 + gx * 0.25, 0.3 + gy * 0.2, 0.8 - gx * 0.2, 1],
                    });
                }
            }
        },
    },
    {
        name: 'origin',
        setup() {
            efx.setClearColor([0.02, 0.06, 0.1, 1]);
            efx.setCamera2D({ frame: FRAME });
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
            this.tex = efx.createTexture(
                efx.createImageData({ width: 16, height: 8, pixels: pixels }));
        },
        render() {
            // size derivation: 1:1 texture-size draw and a
            // sourceRect-derived size, both untransformed
            efx.drawQuad(24, 20, this.tex);
            efx.drawQuad(60, 20, this.tex, { sourceRect: { x: 0, y: 0, w: 8, h: 8 } });
            // scale applies after the size is determined
            efx.drawQuad(224, 216, this.tex, { size: [64, 16], scale: 3 });
            // origin moves the pivot: rotation 90 around quad-local (4, 4)
            efx.drawQuad(96, 340, this.tex, { size: [128, 64], origin: [4, 4], rotation: 90 });
            // corner pivot with derived size, rotation + scale combined
            efx.drawQuad(460, 320, this.tex, { origin: [0, 0], rotation: 45, scale: 1.5 });
        },
    },
];

let scene = -1;
let frameInScene = 0;

function enter(i) {
    scene = i;
    frameInScene = 0;
    efx.setBlendMode('alpha');
    scenes[scene].setup();
    efx.log('browser hello: golden scene ' + scenes[scene].name);
}

function update() {
    if (scene < 0) {
        enter(0);
        return;
    }
    frameInScene++;
    if (frameInScene >= FRAMES_PER_SCENE) {
        enter((scene + 1) % scenes.length);
    }
}

function render() {
    scenes[scene].render();
    efx.setBlendMode('alpha');
    efx.drawQuad(16, 456, efx.whiteTexture, { size: [608, 8], color: [1, 1, 1, 0.15] });
    const progress = frameInScene / FRAMES_PER_SCENE;
    if (progress > 0) {
        efx.drawQuad(16, 456, efx.whiteTexture, { size: [608 * progress, 8], color: [0.3, 0.8, 1, 1] });
    }
}

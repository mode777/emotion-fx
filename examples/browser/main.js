// Golden gallery (f2b visual demo): cycles the six golden-image scenes
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
            efx.drawQuad(220, 150, 200, 180, efx.whiteTexture, {
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
            efx.drawQuad(60, 60, 160, 160, this.tex);
            efx.drawQuad(280, 60, 160, 160, this.tex, {
                sourceRect: { x: 0, y: 0, w: 2, h: 4 },
            });
            efx.drawQuad(500, 60, 80, 160, this.tex, {
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
            efx.drawQuad(270, 230, 100, 20, efx.whiteTexture, {
                color: [1, 1, 1, 1], rotation: 45, scale: 2.5,
            });
            efx.drawQuad(310, 190, 20, 100, efx.whiteTexture, {
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
            efx.drawQuad(140, 140, 200, 200, this.tex, { color: [0.15, 0.35, 0.15, 1] });
            efx.setBlendMode('alpha');
            efx.drawQuad(260, 180, 200, 200, this.tex, { color: [1, 1, 1, 0.5] });
            efx.setBlendMode('subtractive');
            efx.drawQuad(340, 220, 180, 180, this.tex, { color: [0.4, 0.1, 0.4, 1] });
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
                    efx.drawQuad(200 + gx * 100, 140 + gy * 80, 60, 40, efx.whiteTexture, {
                        color: [0.2 + gx * 0.25, 0.3 + gy * 0.2, 0.8 - gx * 0.2, 1],
                    });
                }
            }
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
    efx.drawQuad(16, 456, 608, 8, efx.whiteTexture, { color: [1, 1, 1, 0.15] });
    const progress = frameInScene / FRAMES_PER_SCENE;
    if (progress > 0) {
        efx.drawQuad(16, 456, 608 * progress, 8, efx.whiteTexture, { color: [0.3, 0.8, 1, 1] });
    }
}

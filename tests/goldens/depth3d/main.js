// F3 golden: depth test — the NEARER cube is recorded FIRST, the farther
// one second; depth (not painter's order) decides the overlap
efx.setClearColor([0.04, 0.06, 0.09, 1]);
efx.setCamera3D({ pos: [0, 1.4, 4.5], target: [0, 0, 0.6], fov: 55 });
const near = efx.createMesh(efx.makeCube({ size: 1.1 }));
const far = efx.createMesh(efx.makeCube({ size: 1.1 }));
function update() {}
function render() {
    // nearer (z = 1.6) recorded first
    efx.drawMesh({ mesh: near,
        transform: efx.mat4.translate(efx.mat4.identity(), [0.35, 0, 1.6]),
        color: [0.95, 0.35, 0.25, 1] });
    // farther (origin) recorded second, overlapping on screen
    efx.drawMesh({ mesh: far,
        transform: efx.mat4.translate(efx.mat4.identity(), [-0.35, 0, 0]),
        color: [0.25, 0.8, 0.45, 1] });
}

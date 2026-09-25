// F3 golden: segmented procedural plane, tilted, over a sphere
efx.setClearColor([0.02, 0.05, 0.08, 1]);
efx.setCamera3D({ pos: [0, 2.6, 4.4], target: [0, -0.2, 0], fov: 55 });
const plane = efx.createMesh(efx.makePlane({ size: 4, segments: 4 }));
const ball = efx.createMesh(efx.makeSphere({ radius: 0.7, segments: 24 }));
const tilt = efx.mat4.rotate(efx.mat4.identity(), 12, [1, 0, 0]);
function update() {}
function render() {
    efx.drawMesh({ mesh: plane, transform: tilt,
                   color: [0.55, 0.62, 0.75, 1] });
    const up = efx.mat4.translate(efx.mat4.identity(), [0, 0.7, 0]);
    efx.drawMesh({ mesh: ball, transform: up,
                   color: [0.95, 0.8, 0.25, 1] });
}

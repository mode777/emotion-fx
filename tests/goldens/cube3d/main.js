// F3 golden: procedural cube, tinted, rotated (primitives + transform)
efx.setClearColor([0.05, 0.05, 0.1, 1]);
efx.setCamera3D({ pos: [0, 1.6, 4.2], target: [0, 0, 0], fov: 60 });
const cube = efx.createMesh(efx.makeCube({ size: 1.4 }));
const yaw = efx.mat4.rotate(efx.mat4.identity(), 35, [0, 1, 0]);
const pitch = efx.mat4.rotate(yaw, 22, [1, 0, 0]);
function update() {}
function render() {
    efx.drawMesh({
        mesh: cube,
        transform: pitch,
        color: [0.95, 0.45, 0.15, 1],
    });
}

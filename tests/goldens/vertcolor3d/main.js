// F3 golden: vertex colors multiply the tint (surface colors attribute)
efx.setClearColor([0.03, 0.07, 0.06, 1]);
efx.setCamera3D({ pos: [0, 0, 3], target: [0, 0, 0], fov: 55 });
const P = [
    -1.1, -0.9, 0,
     1.1, -0.9, 0,
     0.0,  1.1, 0,
];
const C = [
    1, 0, 0, 1,
    0, 1, 0, 1,
    0, 0.3, 1, 1,
];
const tri = efx.createMesh(efx.createMeshData({
    positions: P,
    normals: [0, 0, 1, 0, 0, 1, 0, 0, 1],
    colors: C,
    indices: [0, 1, 2],
}));
const triTinted = efx.createMesh(efx.createMeshData({
    positions: P,
    colors: C,
    indices: [0, 1, 2],
}));
function update() {}
function render() {
    const right = efx.mat4.translate(efx.mat4.identity(), [-1.2, 0, 0]);
    efx.drawMesh({ mesh: tri, transform: right });                    // white tint
    const left = efx.mat4.translate(efx.mat4.identity(), [1.25, 0, 0]);
    efx.drawMesh({ mesh: triTinted, transform: left,
                   color: [1, 0.6, 0.5, 1] });                       // warm tint
}

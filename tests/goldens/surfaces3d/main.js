// F3 golden: one mesh, two surfaces — each surface has its own arrays and
// its own place in 3D; drawMesh draws both in surface order (depth orders
// the overlap: surface 1 is nearer and covers surface 0's center)
efx.setClearColor([0.08, 0.05, 0.11, 1]);
efx.setCamera3D({ pos: [0, 0, 4], target: [0, 0, 0], fov: 55 });
function card(z, s, rgb) {
    return {
        positions: [-s, -s, z, s, -s, z, s, s, z, -s, s, z],
        colors: [rgb[0], rgb[1], rgb[2], 1, rgb[0], rgb[1], rgb[2], 1,
                 rgb[0], rgb[1], rgb[2], 1, rgb[0], rgb[1], rgb[2], 1],
        indices: [0, 1, 2, 0, 2, 3],
    };
}
const two = efx.createMesh(efx.createMeshData({
    surfaces: [
        card(0, 1.0, [0.2, 0.55, 0.95]),   // surface 0: far, blue
        card(1.2, 0.55, [1.0, 0.6, 0.1]),  // surface 1: near, orange
    ],
}));
function update() {}
function render() {
    efx.drawMesh({ mesh: two });
}

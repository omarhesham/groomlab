// SPDX-License-Identifier: Apache-2.0
// The voxel grid's block (lookdev_renderer.cpp's GridUniforms), shared by hair_voxelize.comp,
// hair_density.comp and hair_volume.comp. The grid covers the hair's bounds with cubic voxels.

layout(std140, binding = 1) uniform Grid {
    vec4 gridLo;      // xyz: the grid's lowest corner; w: a voxel's size
    vec4 gridParams;  // x: the width scale; y: the hair's opacity; z: the density's fixed point scale; w: the hairs a unit of density crossed counts for
    uvec4 gridDims;   // xyz: voxels along each axis; w: unused
    uvec4 gridCounts; // x: points; y: threads a row of the dispatch; z, w: unused
    vec4 gridToward;  // xyz: toward the light the volume is for (hair_volume.comp); w: unused
};

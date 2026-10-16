// SPDX-License-Identifier: Apache-2.0
#pragma once

// Meshes the lookdev viewport shades with a SurfaceMaterial: the groom's scalp, props loaded
// beside it (a scanned head under the groom) and eyeballs made here. A surface is a ScalpMesh
// (points, triangles, UVs) with normals per corner when its file gave them, occlusion baked
// per vertex by rays against the scene's meshes, and its material. The renderer turns one
// into interleaved vertices with tangents (buildSurfaceVertices) and draws it in the HDR pass
// and in every deep opacity map (it shadows the hair, the hair shadows it). No Qt here: the
// tests build and check them without a device.

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "groomlab/graph.h"  // MeshPtr
#include "groomlab/scalp_mesh.h"
#include "look.h"

namespace groomlab::app {

struct Surface {
    std::string name;
    MeshPtr mesh;                       // positions, triangles, corner UVs
    std::vector<Vec3f> cornerNormals;   // 3 per triangle (empty: the mesh's smooth normals)
    std::vector<float> vertexOcclusion; // per mesh point, 1 open (empty: none baked)
    SurfaceMaterial material;
    bool castsShadows = true;
    // Eyes: the frame the shader reads the iris and sclera in (makeEyeSurface fills it).
    bool isEye = false;
    Vec3f eyeCenter, eyeForward{0.0f, 0.0f, 1.0f}, eyeUp{0.0f, 1.0f, 0.0f};
    float eyeRadius = 1.2f;
    std::uint64_t version = 1;          // bumped when the geometry changes (the GPU copy follows)
};
using SurfacePtr = std::shared_ptr<Surface>;

// The GPU layout: per vertex position (3), normal (3), tangent (4: xyz and the bitangent's
// sign), uv (2), occlusion (1), curvature (1): 14 floats. Vertices are split where corners
// disagree (a normal or a UV seam) and shared where they agree. The tangents follow the UVs
// (Lengyel 2001, "Computing Tangent Space Basis Vectors for an Arbitrary Mesh"), accumulated
// per vertex and made orthogonal to the normal; a mesh without UVs gets any tangent across its
// normal (the normal map is not read then). The curvature (1 / radius, per scene unit) is the
// mean over a point's edges of how fast the smooth normal turns along them: what the skin's
// pre-integrated diffuse looks its profile up by (Penner 2011 derives it from the normal's
// screen derivatives; baked per vertex it is smooth across triangles).
struct SurfaceVertices {
    static constexpr int kStride = 14;
    std::vector<float> data;
    std::vector<std::uint32_t> indices;
    std::size_t vertexCount() const { return data.size() / kStride; }
};
SurfaceVertices buildSurfaceVertices(const Surface& surface);

// Ambient occlusion per point of `mesh` by `rays` cosine-distributed rays from each point (its
// smooth normal's hemisphere, a little off the surface) against `occluders` (the mesh itself
// among them when it shadows itself) within `maxDistance`: 1 where every ray escapes.
std::vector<float> bakeVertexOcclusion(const ScalpMesh& mesh, const std::vector<MeshPtr>& occluders, int rays,
                                       float maxDistance);

// An eyeball at `center` looking along `forward` (unit) with `up` across it, of `radius`
// (1.2 cm is a human eye): a sphere whose front, inside the limbus (the iris' edge, 0.49 of
// the radius out), bulges as the cornea does (a smaller sphere, 0.65 of the radius, meeting the
// sclera at the limbus; the apex 0.095 radii proud), with latitude-longitude UVs and the
// frame the eye shader reads. `rings` x `segments` triangles.
Surface makeEyeSurface(const Vec3f& center, const Vec3f& forward, const Vec3f& up, float radius,
                       const SurfaceMaterial& material, int rings = 48, int segments = 64);

// The cornea's apex ahead of the center and the iris plane's distance from it, in radii
// (the shader's numbers; the tests check the mesh against them).
constexpr float kEyeCorneaApex = 1.0954f;
constexpr float kEyeIrisPlane = 0.84f;
constexpr float kEyeLimbusCos = 0.871f;  // cos of the limbus' polar angle (sin = the iris radius 0.49)
constexpr float kEyeCorneaRadius = 0.65f;

}  // namespace groomlab::app

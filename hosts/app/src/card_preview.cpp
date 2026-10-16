// SPDX-License-Identifier: Apache-2.0
#include "card_preview.h"

#include <vector>

namespace groomlab::app {

SurfacePtr makeCardSurface(const CardMesh& cards, const std::string& previewPng, float roughness) {
    auto surface = std::make_shared<Surface>();
    surface->name = "cards";
    std::vector<ScalpMesh::Triangle> triangles;
    std::vector<ScalpMesh::UV> uvs;
    triangles.reserve(cards.quads.size() / 2);
    uvs.reserve(cards.quads.size() / 4 * 6);
    surface->cornerNormals.reserve(cards.quads.size() / 4 * 6);
    const bool hasNormals = cards.normals.size() == cards.positions.size();
    const bool hasUVs = cards.u.size() == cards.positions.size() && cards.v.size() == cards.positions.size();
    auto corner = [&](std::uint32_t i) {
        if (hasUVs) uvs.push_back({cards.u[i], 1.0f - cards.v[i]});
        surface->cornerNormals.push_back(hasNormals ? cards.normals[i] : Vec3f{0.0f, 1.0f, 0.0f});
    };
    for (std::size_t q = 0; q + 3 < cards.quads.size(); q += 4) {
        const std::uint32_t a = cards.quads[q], b = cards.quads[q + 1], c = cards.quads[q + 2], d = cards.quads[q + 3];
        triangles.push_back({static_cast<std::int32_t>(a), static_cast<std::int32_t>(b), static_cast<std::int32_t>(c)});
        corner(a);
        corner(b);
        corner(c);
        triangles.push_back({static_cast<std::int32_t>(a), static_cast<std::int32_t>(c), static_cast<std::int32_t>(d)});
        corner(a);
        corner(c);
        corner(d);
    }
    auto mesh = std::make_shared<ScalpMesh>(cards.positions, std::move(triangles));
    if (hasUVs) mesh->setCornerUVs(std::move(uvs));
    surface->mesh = mesh;
    // Lit as a plain dielectric in the map's colors: the bake holds the hair's color and its
    // occlusion; the look's roughness keeps the highlight where the strands have it.
    SurfaceMaterial m;
    m.kind = SurfaceKind::Default;
    m.baseColor = {1.0f, 1.0f, 1.0f};
    m.albedoMap = previewPng;
    m.roughness = roughness;
    m.metallic = 0.0f;
    m.specular = 0.3f;
    // The coverage in the map's alpha cuts the gaps between the baked strands away (Maya's
    // card shader's cutoff): hard, depth-correct edges, in the shadow maps too.
    m.alphaCutoff = kCardAlphaCutoff;
    surface->material = m;
    return surface;
}

}  // namespace groomlab::app

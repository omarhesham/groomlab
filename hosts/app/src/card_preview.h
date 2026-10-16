// SPDX-License-Identifier: Apache-2.0
#pragma once

// The baked cards as the viewport shows them: a surface (surface.h) made of the card mesh with
// the bake's preview map as its albedo, drawn by the lookdev path like a prop (lit, shadowed,
// two-sided), so a baked groom is checked where the strands are. Before a bake the cards draw
// as the display builder's flat item instead (DisplaySettings::drawCards). No Qt here.

#include <string>

#include "groomlab/cards.h"
#include "surface.h"

namespace groomlab::app {

// The coverage below which a card's pixel is cut away (Maya's card shader's cutoff).
constexpr float kCardAlphaCutoff = 0.5f;

// A surface of `cards` (two triangles a quad, the card normals per corner, the atlas UVs with V
// flipped: the maps' row 0 is the atlas' v = 1) textured by `previewPng` (the RGBA preview a
// bake writes: color x occlusion, the coverage in alpha), shaded with the hair look's roughness
// and cut out where the coverage is below kCardAlphaCutoff.
SurfacePtr makeCardSurface(const CardMesh& cards, const std::string& previewPng, float roughness);

}  // namespace groomlab::app

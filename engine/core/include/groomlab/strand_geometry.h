// SPDX-License-Identifier: Apache-2.0
#pragma once

// Strands as plain geometry for hosts: which strands to keep under a budget, and the strands
// as one polygon mesh (tubes or flat ribbons) with UVs. Used by the Maya utilities that turn a
// groom into curves or a mesh (groomLabConvert).

#include <cstddef>
#include <vector>

#include "groomlab/feathers.h"
#include "groomlab/strand_set.h"

namespace groomlab {

class ScalpMesh;

// At most `maxCount` strands of `strands`, by ID: the strands whose hashed IDs are lowest,
// the ones a lower preview density keeps longest (keepAtDensity), so a smaller budget keeps a
// subset of a larger one's and nothing depends on the strands' order. Indices ascending; every
// strand when there are no more than maxCount.
std::vector<std::size_t> thinStrandsById(const StrandSet& strands, std::size_t maxCount);

struct StrandMeshOptions {
    enum class Style { Tube, Ribbon };
    Style style = Style::Tube;
    int sides = 4;            // a tube's sides (3 or more)
    float widthScale = 1.0f;  // times each point's width (a diameter)
    float minWidth = 0.0f;    // no ring narrower than this (0: as the widths say)
    bool strandUVs = false;   // also lay every strand out in its own cell of a grid (uv2)
};

// The strands as one mesh (RachisMesh's layout, buildRachisMesh's geometry): per strand a ring
// at every point, `sides` vertices round a tube (a fan closing the tip) or two across a ribbon,
// the ring's width the point's width times widthScale. A ribbon lies in the plane of the
// strand and its side vector: the scalp's normal at the root (when the strand is rooted and a
// scalp is given, else any direction across the root), carried along by rotation-minimizing
// frames, so ribbons lie flat along the scalp. UVs: u across (0..1 round a tube, a seam
// column of its own), v root (0) to tip (1) by arc length. `featherIndex` holds each vertex's
// strand (its index in `strands`), `faceFeather` each face's. With strandUVs, u2/v2 (parallel
// to mesh.u/v, the same faceUVs) put strand k in cell k of a ceil(sqrt(n)) square grid.
// Strands with fewer than two points are skipped.
struct StrandMesh {
    RachisMesh mesh;
    std::vector<float> u2, v2;
};
StrandMesh buildStrandMesh(const StrandSet& strands, const ScalpMesh* scalp, const StrandMeshOptions& options);

}  // namespace groomlab

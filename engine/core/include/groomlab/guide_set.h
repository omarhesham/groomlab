// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "groomlab/math.h"
#include "groomlab/strand_set.h"

namespace groomlab {

class ScalpMesh;

// A guide curve. points[0] is the root end.
struct Guide {
    std::uint64_t id = 0;
    StrandRoot root;  // set by GuideSet::rootOnMesh
    std::vector<Vec3f> points;
    // Per-guide values (GH-7), e.g. painted "clump"; Interpolate blends them
    // into strand channels of the same name.
    std::map<std::string, float> attributes;
    // Per-point layers: one value per point of `points`, painted along the
    // guide; Interpolate blends them into point channels of the same name.
    std::map<std::string, std::vector<float>> pointAttributes;
};

// Whether a curve was drawn tip-first: its last point nearer the mesh than its
// first by more than 2% of its length (a curve with both ends on the mesh, a
// path drawn on it, starts at its root).
bool drawnTipFirst(const std::vector<Vec3f>& points, const ScalpMesh& mesh);

// Moves a curve rigidly so its first point sits on the closest point of the
// mesh; returns that root. Points must not be empty.
StrandRoot rootRigidly(std::vector<Vec3f>& points, const ScalpMesh& mesh);

// Flat set of guides: curves for Interpolate and Clump. The editable parent/child
// hierarchy is GuideHierarchy (guide_hierarchy.h), which flattens into these.
class GuideSet {
public:
    std::size_t size() const { return guides_.size(); }
    bool empty() const { return guides_.empty(); }

    Guide& add(std::uint64_t id, std::vector<Vec3f> points);
    const std::vector<Guide>& guides() const { return guides_; }
    std::vector<Guide>& guides() { return guides_; }

    // Roots every guide on the closest scalp point and moves the curve rigidly
    // so its first point sits exactly on the surface (GH-2).
    void rootOnMesh(const ScalpMesh& mesh);

    // Reverses guides whose last point is nearer the scalp than their first,
    // so curves drawn tip-first still root correctly, then roots them.
    void orientAndRoot(const ScalpMesh& mesh);

    std::uint64_t contentHash() const;

    // The color these guides draw in when they are shown as guides (a ToGuides node's `color`,
    // carried by copies: a Sculpt node's editable guides). Display only: not part of the hash.
    bool hasDisplayColor() const { return hasDisplayColor_; }
    const Vec3f& displayColor() const { return displayColor_; }
    void setDisplayColor(const Vec3f& linearRgb) {
        displayColor_ = linearRgb;
        hasDisplayColor_ = true;
    }

private:
    std::vector<Guide> guides_;
    Vec3f displayColor_{0, 0, 0};
    bool hasDisplayColor_ = false;
};

}  // namespace groomlab

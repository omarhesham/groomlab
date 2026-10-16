// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "groomlab/frames.h"
#include "groomlab/guide_set.h"
#include "groomlab/math.h"
#include "groomlab/strand_set.h"
#include "groomlab/transfer.h"

namespace groomlab {

class ScalpMesh;

using GuideId = std::uint64_t;
using GroupId = std::uint32_t;
constexpr GuideId kNoGuide = 0;
constexpr GroupId kNoGroup = 0;

// A guide's tube: the volume its child guides are generated in (GH-3).
struct TubeProfile {
    std::vector<float> radius{0.1f, 0.1f};  // scene units, evenly spaced root to tip, linear between
    float twist = 0.0f;                     // radians, root to tip; rotates the children's cross-section

    float radiusAt(float t) const;
};

// A named set of guides. Groups form a tree (GH-1): each child group's guides
// have their parents in the parent group. Depth 0 groups hold primary guides.
struct GuideGroup {
    GroupId id = kNoGroup;
    std::string name;
    GroupId parent = kNoGroup;
    // The color the group's guides draw in (a Sculpt node's editable guides take their ToGuides
    // node's): display only, not saved; a host's per-group color (guideGroupColors) wins.
    bool hasColor = false;
    std::array<float, 3> color{0, 0, 0};
};

struct HierarchyGuide {
    GuideId id = kNoGuide;
    GroupId group = kNoGroup;
    GuideId parent = kNoGuide;  // kNoGuide for primary guides
    bool locked = false;        // locked children keep their own world shape (GH-6)
    StrandRoot root;
    TubeProfile tube;
    // Reference direction for this guide's first frame normal (projected
    // perpendicular to the root tangent). Fixed at creation so the frame, and
    // with it every child, turns smoothly when the guide is reshaped.
    Vec3f frameUp{1, 0, 0};
    std::map<std::string, float> attributes;  // per-guide data for downstream operators (GH-7)
    // Per-point layers: one value per point of `points`, painted along the
    // guide (the pointwise Attribute brush). Children without their own take
    // the nearest ancestor's, resampled along it; Interpolate carries them
    // onto the strands as point channels.
    std::map<std::string, std::vector<float>> pointAttributes;

    // World-space shape. Authoritative for primary and locked guides until
    // they are bound to the scalp; for following children it is recomputed by
    // GuideHierarchy::evaluate.
    std::vector<Vec3f> points;

    // Primary and locked guides, once rooted (GH-2): the shape and frameUp in
    // the root's surface frame (x along the root face's first edge, y along
    // the surface normal). evaluate() rebuilds world points from these, so
    // guides move and turn with a deforming scalp. Empty until first rooted;
    // cleared by setPoints so the next evaluate re-binds.
    std::vector<Vec3f> rootLocal;
    Vec3f frameUpLocal{1, 0, 0};

    // Following children only (GH-5): shape relative to the parent, one entry
    // per parent sample t = i / (n - 1). x and y are cross-section coordinates
    // in the parent's rotation-minimizing frame, in units of the parent's tube
    // radius; z is along the parent's tangent, in scene units.
    std::vector<Vec3f> offsets;

    bool isChild() const { return parent != kNoGuide; }
    bool followsParent() const { return isChild() && !locked; }
    // The guide's rotation-minimizing frames along `points`, starting from frameUp.
    std::vector<Frame> frames() const;
};

// Settings for GuideHierarchy::generateChildren (GH-4).
struct ChildGenerationParams {
    std::size_t count = 5;         // children per parent guide
    float rootSpread = 0.8f;       // root disc radius, in parent tube radii
    float tipSpread = 1.0f;        // tip disc radius, in parent tube radii
    float twist = 0.0f;            // radians, extra rotation of each child around the parent, root to tip
    float jitter = 0.1f;           // random tip offset, in parent tube radii
    float childTubeScale = 0.35f;  // child tube radius as a fraction of the parent's
    std::uint64_t seed = 0;
};

// Tonic-style guide hierarchy: primary guides, children generated inside
// their parents' tubes, grandchildren inside the children's tubes, and so on.
// Children store offsets in their parent's frame, so editing a parent carries
// every descendant with it.
class GuideHierarchy {
public:
    // ---- Groups ---------------------------------------------------------
    GroupId addGroup(const std::string& name, GroupId parent = kNoGroup);
    // A group's own draw color (GuideGroup::color; display only).
    void setGroupColor(GroupId id, const std::array<float, 3>& linearRgb);
    const std::vector<GuideGroup>& groups() const { return groups_; }
    const GuideGroup* group(GroupId id) const;
    GroupId findGroup(const std::string& name) const;  // kNoGroup if absent
    int groupDepth(GroupId id) const;                   // 0 for root groups, -1 if unknown
    int maxDepth() const;                               // -1 when there are no groups

    // Removes a group, its descendant groups and all their guides.
    void removeGroup(GroupId id);
    // Throws std::invalid_argument when the name is empty or taken.
    void renameGroup(GroupId id, const std::string& name);

    // ---- Guides ---------------------------------------------------------
    // Adds a primary guide to a root group. IDs are assigned when id is kNoGuide.
    GuideId addGuide(GroupId group, std::vector<Vec3f> points, TubeProfile tube = {}, GuideId id = kNoGuide);
    // Adds every guide of a flat set (a node's Guides output, ToGuides say) as
    // primaries of a root group, with their attributes as painted layers.
    // Guides keep their IDs when free, else get new ones; guides with fewer
    // than 2 points are skipped. Returns the IDs added. Call evaluate after,
    // to root them on the scalp.
    std::vector<GuideId> addGuides(GroupId group, const GuideSet& guides, TubeProfile tube = {});
    const std::vector<HierarchyGuide>& guides() const { return guides_; }
    const HierarchyGuide* guide(GuideId id) const;
    std::vector<GuideId> guidesInGroup(GroupId group) const;
    std::vector<GuideId> childrenOf(GuideId parent) const;

    // Generates `params.count` children for each of `parents` (default: every
    // guide of `parentGroup`) into the group `childGroupName` under
    // parentGroup, which is created if missing and added to otherwise. A parent
    // that already has children there gets new ones in their place (their own
    // descendants go too), so artists can pick, parent by parent, which guides
    // have children. Returns the child group. Parent shapes must be current
    // (call evaluate first after editing).
    GroupId generateChildren(GroupId parentGroup, const std::string& childGroupName,
                             const ChildGenerationParams& params, const std::vector<GuideId>* parents = nullptr);

    // Adds a child of `parent` to `group` (a child group of the parent's group)
    // with the given world shape, e.g. placed by hand. It follows its parent
    // from then on, keeping this shape relative to it. Without a tube (or with
    // one of no radii) it takes the parent's, scaled by 0.35. Returns its ID.
    GuideId addChildGuide(GroupId group, GuideId parent, std::vector<Vec3f> points,
                          std::optional<TubeProfile> tube = std::nullopt);

    // ---- Evaluation and editing -----------------------------------------
    // Binds unbound primary and locked guides to the scalp (flipping tip-first
    // primaries, then rooting at the closest point) and rebuilds bound ones
    // from their root frames, so they follow the scalp. Then recomputes every
    // following child top-down from its parent's frames and snaps its root
    // onto the scalp. Without a scalp, guides keep their current shapes.
    // A scalp with other triangles than the one the guides were bound on (a
    // re-meshed or subdivided head) unbinds them first: each keeps its world
    // shape and re-roots at the closest point, instead of being rebuilt on
    // the wrong face.
    void evaluate(const ScalpMesh* scalp);
    // The topology (ScalpMesh::topologyHash) the bound guides' root frames
    // belong to; 0 until bound.
    std::uint64_t boundTopology() const { return boundTopology_; }

    // Same as evaluate, restricted to the given guides and their descendants:
    // the cheap path after editing a few guides (brush strokes). The scalp
    // must be the one the rest of the hierarchy was last evaluated on.
    void evaluateSubtrees(const ScalpMesh* scalp, const std::vector<GuideId>& changed);

    // Sets a guide's world points for this evaluation only, e.g. a simulated
    // pose. Its binding and offsets are kept, so the next evaluate() restores
    // the authored shape. Follow with evaluateDescendants.
    void pose(GuideId id, std::vector<Vec3f> points);
    // The same with the frame reference given (a guide moved rigidly, follow_scalp.h) rather than
    // carried along with the root direction.
    void pose(GuideId id, std::vector<Vec3f> points, const Vec3f& frameUp);
    // Re-evaluates the descendants of `guides`, not the guides themselves.
    void evaluateDescendants(const ScalpMesh* scalp, const std::vector<GuideId>& guides);

    // Hierarchy tools. Every guide keeps its current world shape; what changes
    // is what it follows. Throw std::invalid_argument on a bad request.
    //
    // Moves guides under another parent guide, which must be in their
    // group's parent group.
    void reparent(const std::vector<GuideId>& guides, GuideId newParent);
    // Moves a group one level up: its guides follow their grandparents (a
    // group reaching the root: its guides become primaries rooted on the
    // scalp at the next evaluate). Sub-groups move with it.
    void promoteGroup(GroupId group);
    // Moves a group one level down, under `newParent`, a group with the same
    // parent; each guide follows the guide of newParent whose root is nearest.
    void demoteGroup(GroupId group, GroupId newParent);
    // Mirrors a root group across the plane where coordinate `axis` (0 x,
    // 1 y, 2 z) equals `center`: guides whose roots are on the source side
    // (positive: above center) are copied to the other side, replacing the
    // guides there (and their descendants). A copy's frames (frameUp) are the
    // mirror image of its source's, as in mirrorAll. Returns the new guide IDs.
    std::vector<GuideId> mirrorGroup(GroupId group, int axis, float center, bool fromPositive);
    // Symmetry sync: makes the whole groom symmetric across the plane. Every
    // root group is mirrored as mirrorGroup does, and each copied guide takes
    // mirror images of its descendants (children, grandchildren... in their
    // own groups, attributes and locks kept), so hand-edited children survive.
    // Guides on the plane are left alone. Needs an evaluated hierarchy (the
    // children's world shapes). Returns the new guide IDs.
    std::vector<GuideId> mirrorAll(int axis, float center, bool fromPositive);
    // Moves every guide onto another scalp (GH-8). Evaluates on `from`, then
    // each primary and locked guide keeps its shape, turned with the surface
    // normal, and is re-rooted at the closest point of `to` to fit.apply(its
    // root). Children follow their parents. Both meshes at their current
    // positions; the hierarchy is left evaluated on `to`.
    void transfer(const ScalpMesh& from, const ScalpMesh& to, const ScalpFit& fit = {});
    // The same by any mapping (transfer.h) between the two meshes it was built on: each primary
    // and locked guide re-rooted where the mapping takes its root, its shape carried by the
    // mapping's local rotation and scale there (closest and bounds: as above); tube radii and
    // children's offsets along scale with the guide they hang from (keepLengths: they keep theirs).
    void transfer(const ScalpMapping& mapping);
    // Removes guides and their descendants.
    void removeGuides(const std::vector<GuideId>& guides);

    // Detaches a child: it keeps its current world shape from now on.
    void lock(GuideId id);
    // Re-attaches a child: its current world shape becomes offsets.
    void unlock(GuideId id);
    // Replaces a guide's world shape (brush edits). Following children convert
    // it to offsets; their children follow on the next evaluate. Per-point
    // layers are resampled to the new point count.
    void setPoints(GuideId id, std::vector<Vec3f> points);
    void setTube(GuideId id, TubeProfile tube);
    void setAttribute(GuideId id, const std::string& name, float value);
    // Sets a per-point layer: `values` are resampled to the guide's point count
    // when they differ. Empty values remove the layer from the guide.
    void setPointAttribute(GuideId id, const std::string& name, std::vector<float> values);

    // ---- Output ---------------------------------------------------------
    // Guides of every group at `level` (-1: the deepest level), as a flat set
    // for operators such as Interpolate.
    GuideSet flatten(int level = -1) const;
    // A guide's attributes over those it inherits from its ancestors.
    std::map<std::string, float> effectiveAttributes(const HierarchyGuide& g) const;
    // One attribute as effectiveAttributes would give it (the nearest guide up
    // the chain that sets it); false when none does. No map is built.
    bool effectiveAttribute(const HierarchyGuide& g, const std::string& name, float* value) const;
    bool hasAttributes() const;  // any guide carries an attribute
    // Per-point layers (one value per guide point). A guide's effective layers
    // are its own over those of its ancestors, each resampled along the guide
    // to its point count (a child follows its parent point for point).
    std::map<std::string, std::vector<float>> effectivePointAttributes(const HierarchyGuide& g) const;
    // One per-point layer as effectivePointAttributes gives it, resampled to
    // g's point count; false when no guide up the chain carries it.
    bool effectivePointAttribute(const HierarchyGuide& g, const std::string& name, std::vector<float>* values) const;
    bool hasPointAttributes() const;  // any guide carries a per-point layer
    std::vector<std::string> pointAttributeNames() const;
    // Attribute layers of both kinds: every name any guide carries, sorted;
    // removing one from every guide; renaming one (throws if `to` exists).
    std::vector<std::string> attributeNames() const;
    void removeAttribute(const std::string& name);
    void renameAttribute(const std::string& from, const std::string& to);
    GuideSet flattenGroup(GroupId group) const;

    // Everything the hierarchy holds, hashed. Worked out once and kept until the next change:
    // every non-const member clears it (a new one must too), so hosts may call it on every
    // evaluation. computeContentHash works it out now, without the memo (tests check the two).
    std::uint64_t contentHash() const;
    std::uint64_t computeContentHash() const;

    // JSON round trip, for storage on the Maya node.
    std::string toJson() const;
    static GuideHierarchy fromJson(const std::string& text);  // throws std::runtime_error

private:
    HierarchyGuide* mutableGuide(GuideId id);
    GuideId nextGuideId();
    // `parentFrames`: the parent's rotation-minimizing frames when already known (null: computed).
    void resolveFollower(HierarchyGuide& child, const HierarchyGuide& parent, const ScalpMesh* scalp,
                         const std::vector<Frame>* parentFrames = nullptr);
    // Evaluates guides in `order` (parents first), computing each parent's frames once for all its followers.
    void evaluateInOrder(const std::vector<std::size_t>& order, const ScalpMesh* scalp);
    void evaluateGuide(HierarchyGuide& g, const ScalpMesh* scalp, const std::vector<Frame>* parentFrames = nullptr);
    std::vector<std::size_t> depthOrder(const std::vector<std::size_t>& indices) const;
    std::vector<std::size_t> subtrees(const std::vector<GuideId>& roots, bool includeRoots) const;
    std::vector<Vec3f> offsetsFromWorld(const HierarchyGuide& child, const HierarchyGuide& parent) const;
    void rebuildIndex();
    void invalidateHash() { hashMemo_.valid.store(false, std::memory_order_relaxed); }

    // contentHash's memo. Copies carry it (the same content).
    struct HashMemo {
        mutable std::atomic<bool> valid{false};
        mutable std::atomic<std::uint64_t> value{0};
        HashMemo() = default;
        HashMemo(const HashMemo& o) : valid(o.valid.load(std::memory_order_acquire)), value(o.value.load()) {}
        HashMemo& operator=(const HashMemo& o) {
            const bool known = o.valid.load(std::memory_order_acquire);  // before the value (stored after it)
            value.store(o.value.load());
            valid.store(known);
            return *this;
        }
    };

    HashMemo hashMemo_;
    std::vector<GuideGroup> groups_;
    std::vector<HierarchyGuide> guides_;
    std::unordered_map<GuideId, std::size_t> index_;
    GroupId nextGroupId_ = 1;
    GuideId nextGuideId_ = 1;
    std::uint64_t boundTopology_ = 0;
};

}  // namespace groomlab

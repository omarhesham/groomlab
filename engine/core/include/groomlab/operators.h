// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <string>

namespace groomlab {

class OperatorRegistry;

// Built-in operator type names (the "type" field in graph JSON).
namespace op {
constexpr const char* kScalp = "Scalp";              // external "scalp" -> Mesh
constexpr const char* kColliderMesh = "ColliderMesh";      // external "mesh:<name>" -> Mesh (a collider, say)
constexpr const char* kGuides = "Guides";            // external "guides" -> Guides
constexpr const char* kScatter = "Scatter";          // Mesh -> Strands
constexpr const char* kInterpolate = "Interpolate";  // Strands + Guides -> Strands
constexpr const char* kLength = "Length";            // Strands -> Strands
constexpr const char* kWidth = "Width";              // Strands -> Strands
constexpr const char* kResample = "Resample";        // Curves -> Curves (segments per strand)
constexpr const char* kOutput = "Output";            // Strands -> Strands (graph result)
constexpr const char* kClump = "Clump";              // Strands + Guides (centers) -> Strands
constexpr const char* kBraid = "Braid";              // Strands + Guides (braid paths) -> Strands
constexpr const char* kInstance = "Instance";        // Strands -> Strands (geometry instanced on them)
constexpr const char* kScales = "Scales";            // Strands -> Strands (a scale at each root, on the skin)
constexpr const char* kToStrands = "ToStrands";      // Guides -> Strands (each guide a strand)
constexpr const char* kToGuides = "ToGuides";        // Strands -> Guides (procedural guide curves)
constexpr const char* kExpression = "Expression";    // -> Field (a per-strand expression)
constexpr const char* kMap = "Map";                  // -> Field (a map painted on the scalp: guide layer, vertex map or texture; or an image)
constexpr const char* kFieldNoise = "FieldNoise";    // -> Field (3D noise over the scalp)
constexpr const char* kFieldRemap = "FieldRemap";    // Field -> Field (range and curve)
constexpr const char* kFieldCombine = "FieldCombine";  // Field + Field -> Field
constexpr const char* kCurve = "Curve";              // -> Curve (a reusable ramp for ramp params)
constexpr const char* kControl = "Control";          // -> Field (a host value by name: a keyable slider in Maya)
constexpr const char* kNoise = "Noise";              // Strands -> Strands
constexpr const char* kCurl = "Curl";                // Strands -> Strands
constexpr const char* kTwist = "Twist";              // Strands (+ Guides axes) -> Strands (turned around an axis)
constexpr const char* kSmooth = "Smooth";            // Curves -> Curves (bumps up to a size relaxed, at 1 straight; lengths kept)
constexpr const char* kBlendNeighbors = "BlendNeighbors";  // Curves -> Curves (each strand toward its neighbours' mean shape)
constexpr const char* kRestoreLength = "RestoreLength";    // Curves (+ Curves reference) -> Curves (segment lengths restored)
constexpr const char* kCull = "Cull";                // Strands -> Strands (thinned by a map)
constexpr const char* kHairColor = "HairColor";  // Strands -> Strands (melanin, linear RGB and highlight channels)
constexpr const char* kBands = "Bands";          // Strands -> Strands (bands of another pigment along each hair: fur)
constexpr const char* kFeatherColor = "FeatherColor";  // Strands -> Strands (feather patterns, down color, iridescence)
constexpr const char* kFlyaway = "Flyaway";      // Strands -> Strands (stray hairs)
constexpr const char* kStatic = "Static";        // Strands -> Strands (charged hair repels itself: puffed out, tips lifted, locks frayed)
constexpr const char* kTangle = "Tangle";        // Strands -> Strands (mats, wet strands in pairs)
constexpr const char* kTie = "Tie";              // Strands -> Strands (ponytails: strands along the scalp to a ring, through it in order, fanned into a tail)
constexpr const char* kWet = "Wet";              // Strands -> Strands (wet hair: small clumps near the root merging into larger ones toward the tip)
constexpr const char* kCards = "Cards";          // Strands -> Strands (+ game cards from these strands)
constexpr const char* kFlow = "Flow";            // Strands -> Strands (laid along the flow arrows on the scalp)
constexpr const char* kAttribute = "Attribute";      // Strands -> Strands (adds a channel)
constexpr const char* kMerge = "Merge";              // Strands + Strands -> Strands
constexpr const char* kSwitch = "Switch";            // Strands | Strands -> Strands
constexpr const char* kBlend = "Blend";              // Strands + Strands -> Strands (two looks of the same strands)
constexpr const char* kSelfCollide = "SelfCollide";  // Strands -> Strands (overpacked hair pushed apart: no strand through another)
constexpr const char* kCollide = "Collide";          // Strands + Mesh (+ Mesh collider) -> Strands (push out of the meshes)
constexpr const char* kPart = "Part";                // Strands -> Strands (opens the parts)
constexpr const char* kGravity = "Gravity";          // Curves -> Curves (bent from the root under their weight)
constexpr const char* kDisplace = "Displace";        // Curves -> Curves (offset along the scalp's normal or a direction)
constexpr const char* kMirror = "Mirror";            // Curves -> Curves (mirrored across a plane, rooted again on the scalp)
constexpr const char* kWind = "Wind";                // Curves -> Curves (bent by a wind moving through space and time)
constexpr const char* kJiggle = "Jiggle";            // Curves -> Curves (lag and overshoot from the scalp's recent motion)
constexpr const char* kSculpt = "Sculpt";            // Guides (+ Mesh scalp) -> Guides (brush strokes kept in the node: sculpt.h)
constexpr const char* kSimulate = "Simulate";        // Guides (+ Mesh collider) -> Guides (the hair solver's frame: simulation.h)
constexpr const char* kPropagate = "Propagate";      // Curves (shafts) -> Strands (barbs grown off each shaft: feathers)
constexpr const char* kPlume = "Plume";              // Strands (feathers) (+ Mesh collider) -> Strands (laid over one another and off the body)
constexpr const char* kSplit = "Split";              // Curves -> Curves a, b, c, d (shares of the strands, by ID or a map)
constexpr const char* kTransform = "Transform";      // Curves -> Curves (moved, turned, scaled about each root or a pivot)
constexpr const char* kBend = "Bend";                // Curves -> Curves (bent toward a direction between a start and an end)
constexpr const char* kDirection = "Direction";      // Curves -> Curves (turned about the root to an inclination and roll)
constexpr const char* kMeshCut = "MeshCut";          // Curves (+ Mesh) -> Curves (cut where they cross the mesh)
constexpr const char* kRenderData = "RenderData";    // Strands -> Strands (the opacity channel for the shader)
constexpr const char* kCompound = "Compound";        // a subgraph as one node (PG-6)
constexpr const char* kInput = "Input";              // a compound input inside its subgraph
constexpr const char* kGroup = "Group";              // Curves -> Curves (a named 0-1 strand channel: a region, map or guide groups)
constexpr const char* kReference = "Reference";      // -> Strands, Guides (another groom file's result: a library look)
}  // namespace op

// The most earlier frames a Jiggle node reads (its `history` is clamped to 1..this).
constexpr int kMaxScalpHistory = 48;

// What a node type does, in a line (empty for a type it does not know): every host's param panel
// and add-node search show it (operatorSpecsJson's "description").
std::string operatorDescription(const std::string& type);

// Registers every built-in operator. OperatorRegistry::instance() calls this
// on first use.
void registerBuiltinOperators(OperatorRegistry& registry);

// Work the operators keep between evaluations, counted over the process (tests check what the
// memos save): how many times a Scatter relaxed its roots (randomness below 1; a Scatter keeps
// its relaxed roots on the rest pose, so a deforming scalp does not relax them again), and how
// many times an Interpolate bound its strands to guides (each strand's guides and weights, kept
// while the roots, guide roots, parts and binding params stay; a brush dab only re-blends).
std::size_t scatterRelaxCount();
// Relaxation iterations run so far in this process (tests: a settled layout stops early).
std::size_t scatterRelaxIterationCount();
std::size_t interpolateBindCount();
// How many strands Interpolate bindings searched guides for: when only some guide roots moved (a
// brush dab re-roots the guides it touches), the strands no moved guide reaches keep theirs.
std::size_t interpolateStrandBindCount();
// Tests: Interpolate binds and blends as first written (every strand's guides from the kd-tree,
// the blend one point at a time) instead of by its faster paths, which must give the same
// strands bit for bit.
void setInterpolateReferencePaths(bool reference);

}  // namespace groomlab

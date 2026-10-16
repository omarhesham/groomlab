// SPDX-License-Identifier: Apache-2.0
#include <map>
#include <string>

#include "groomlab/graph.h"
#include "groomlab/operators.h"
#include "internal.h"

namespace groomlab {

std::string operatorDescription(const std::string& type) {
    // What each node does, in a line: the top of its param panel and its tooltip in the add-node
    // search, in every host (operatorSpecsJson's "description").
    static const std::map<std::string, std::string> kAbout = {
        {"Attribute", "Writes a per-strand channel for later nodes or the shader: a number, a map or an expression."},
        {"Bands", "Bands of another pigment along each hair, on the guard hairs: agouti, ticked, shaded fur. After HairColor."},
        {"Bend", "Bends strands toward or away from the scalp, or combs them toward a direction, between a start and an end; start and end 0 stand them up at the root."},
        {"Blend", "Mixes two looks of the same strands (dry to wet)."},
        {"BlendNeighbors", "Pulls each strand toward its neighbours' shape for a coherent flow."},
        {"Braid", "Braids strands along paths: three-strand, fishtail, rope, cornrows."},
        {"Cards", "Hair cards for game engines, with baked maps and LODs."},
        {"Clump", "Pulls strands toward their clump's center (or into round locks; Twist after it for ropes)."},
        {"Collide", "Pushes strands out of the scalp and any collider. Best last."},
        {"ColliderMesh", "Another mesh (a body, a collar) for Collide to keep strands out of."},
        {"Compound", "Nodes grouped into one, with the params that matter exposed."},
        {"Control", "A keyable slider on the groom (channel box) driving a param."},
        {"Cull", "Thins strands out: bald or thin patches."},
        {"Curl", "Helical curls around each strand."},
        {"Curve", "A ramp shared by several params."},
        {"Direction", "Points strands by inclination from the scalp and roll around its normal, at the root."},
        {"Displace", "Moves strands along the scalp's normal (or a direction): volume off the scalp."},
        {"Expression", "A value per strand from a formula, feeding any param (or one value, of the frame and its variables)."},
        {"FeatherColor", "Patterns on feathers (bars, spots, an eye, a painted image), a down color and iridescence. After HairColor."},
        {"FieldCombine", "Two fields multiplied, added, mixed..."},
        {"FieldNoise", "Smooth noise over the scalp, feeding any param."},
        {"FieldRemap", "Reshapes a field through a curve."},
        {"Flow", "Lays short hair along the flow arrows painted on the scalp."},
        {"Flyaway", "Stray hairs leaving the flow. After Clump."},
        {"Gravity", "Bends strands from the root under their weight; stiff at the root (a Collide after it keeps them off the scalp)."},
        {"Group", "Names a set of strands (a region, a map, an expression, guide groups) for other nodes' affect to restrict them to."},
        {"Guides", "The groom's guides, by group: what Interpolate, Clump and friends follow."},
        {"HairColor", "The hair's look: pigment and dye, a map, greys, randomness, a tip color and the highlight (specular, roughness, tilt)."},
        {"ImportCurves", "Curves from an Alembic file, as strands or guides."},
        {"Input", "An input of the compound this node lives in."},
        {"Instance", "Copies a mesh (leaves, feathers, beads) along the strands."},
        {"Interpolate", "Grows each root into a strand shaped like the guides around it."},
        {"Jiggle", "Strands lag behind the scalp's motion, overshoot and settle: no simulation cache."},
        {"Length", "Trims strands to a share of their length."},
        {"Map", "A map painted in Groom Tools > Paint (or an image), feeding any param."},
        {"Merge", "Combines two sets of strands."},
        {"MeshCut", "Cuts strands where they cross a mesh: an obstacle, or a haircut's hull."},
        {"Mirror", "Mirrors guides or strands across a plane: groom one half, get both."},
        {"Noise", "Random displacement; on a fraction of the strands at a high frequency, frizz."},
        {"Output", "What the groom shows and renders."},
        {"Part", "Opens a gap along the parts drawn on the scalp."},
        {"Plume", "Lays feathers over one another (by a rank) and off the body: no vane through another."},
        {"Propagate", "Grows barbs off every strand it is fed (the shafts): feathers, ferns, plumes."},
        {"Reference", "Another groom file's result (its strands, or its guides): a library look reused or instanced, where the file has it or bound onto this scalp."},
        {"RenderData", "Render controls per strand for the shader: opacity along the strands."},
        {"Resample", "Evens out the points along each strand."},
        {"RestoreLength", "Gives strands their length back after deformers."},
        {"Scalp", "The mesh the hair grows on."},
        {"Scales", "A scale at each strand's root, lying on the skin along the strand: reptile skin, armor plates."},
        {"Scatter", "Roots over the scalp: how many hairs, and where."},
        {"Simulate", "Hair dynamics on the guides: gravity, wind, the head's motion, collisions. Play, or Simulate Range; bake for renders."},
        {"Sculpt", "Brush the guides the graph made (ToGuides): the strokes are kept in the node and follow the guides when what feeds it changes."},
        {"SelfCollide", "Hair no longer passes through itself: where it is packed tighter than it can be, it is pushed apart, so layers rest on one another."},
        {"Smooth", "Takes out bumps up to a size (a share of the length; 1 straightens), keeping lengths and roots."},
        {"Split", "Sends each strand to one of four outputs by share or by a map: several designs on one set."},
        {"Static", "Hair charged with static repels itself, like dry winter hair: strands spread, the outer layer puffs out, tips lift and locks fray."},
        {"StrandMesh", "The strands it is handed as a polygon mesh (tubes or ribbons) that follows the animation: Maya's groom outStrandMesh, for Alembic or a renderer without curves."},
        {"Switch", "Picks one of several inputs: look variants."},
        {"Tangle", "Strands stuck together: mats and wet hair."},
        {"ToGuides", "Strands become guides, for nodes that follow guides."},
        {"ToStrands", "Guides become strands: render them, or merge them in."},
        {"Tie", "Ponytails and hair ties: strands run along the scalp to a ring, pass through it in order and fan into a tail."},
        {"Transform", "Moves, turns and scales strands about their roots (or the whole set), with randomness."},
        {"Twist", "Twists strands around a lock or clump: ropes, twisted locks."},
        {"Value", "One value for a whole node: a number, integer, switch, vector, color or text that drives any exposed param."},
        {"Width", "Strand thickness, root to tip."},
        {"Wet", "Wet hair: small clumps near the root merging into bigger ones toward the tip, from one wetness slider."},
        {"Wind", "Strands bend along a wind moving through space and time. Play the timeline to see it."},
    };
    const auto it = kAbout.find(type);
    return it == kAbout.end() ? std::string() : it->second;
}

void registerBuiltinOperators(OperatorRegistry& registry) {
    registerSourceOperators(registry);
    registerReferenceOperator(registry);
    registerScatterOperator(registry);
    registerInterpolateOperator(registry);
    registerShapeOperators(registry);
    registerLookOperators(registry);
    registerCurlOperator(registry);
    registerCinematicOperators(registry);
    registerColorOperators(registry);
    registerDeformOperators(registry);
    registerFieldOperators(registry);
    registerInstanceOperator(registry);
    registerBraidOperator(registry);
    registerMirrorOperator(registry);
    registerExtraOperators(registry);
    registerNeighborOperators(registry);
    registerMotionOperators(registry);
    registerSimulateOperator(registry);
    registerCompoundOperators(registry);
    registerFeatherOperators(registry);
    registerTransformOperators(registry);
    registerMeshCutOperator(registry);
    registerRenderDataOperator(registry);
    registerStrandMeshOperator(registry);
    registerGroupOperator(registry);
    registerPlumeOperator(registry);
    registerWetOperator(registry);
    registerTieOperator(registry);
    registerSelfCollideOperator(registry);
    registerStaticOperator(registry);
    registerSculptOperator(registry);
}

}  // namespace groomlab

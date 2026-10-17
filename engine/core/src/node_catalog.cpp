// SPDX-License-Identifier: Apache-2.0
#include "groomlab/node_catalog.h"

#include <cctype>
#include <map>

namespace groomlab {

const std::vector<NodeCategory>& nodeCategories() {
    // What it reads, the strands grown from it, their shape, deformers, collisions, the look
    // (clumps and detail), motion, feathers, color, conversions, fields, plumbing, games.
    static const std::vector<NodeCategory> categories = {
        {"source", "Sources"}, {"grow", "Grow"},         {"shape", "Shape"},           {"deform", "Deform"},
        {"collide", "Collide"}, {"look", "Look"},        {"motion", "Motion"},         {"feathers", "Feathers"},
        {"color", "Color"},    {"convert", "Convert"},   {"field", "Fields & Curves"}, {"utility", "Utility"},
        {"games", "Games"},
    };
    return categories;
}

const std::vector<NodeCatalogEntry>& nodeCatalog() {
    static const std::vector<NodeCatalogEntry> entries = {
        // Sources
        {"Scalp", "source", "", "mesh skin body surface"},
        {"Guides", "source", "", "guide curves groups"},
        {"ColliderMesh", "source", "", "body collar obstacle mesh"},
        {"ImportCurves", "source", "", "alembic abc curves file"},
        {"Reference", "source", "Linked Groom", "reference library instance groom file link"},
        {"Input", "source", "", "compound input", true},
        // Grow
        {"Scatter", "grow", "", "density count roots number hairs"},
        {"Interpolate", "grow", "", "children guides blend strands"},
        {"Cull", "grow", "", "thin bald density remove patches"},
        // Shape
        {"Length", "shape", "", "trim cut shorten haircut"},
        {"Width", "shape", "", "thickness radius taper"},
        {"Resample", "shape", "", "points segments cvs even"},
        {"Direction", "shape", "", "inclination roll angle orient"},
        {"Flow", "shape", "", "arrows comb short hair direction"},
        {"Part", "shape", "", "parting gap line"},
        {"Smooth", "shape", "", "relax straighten bumps"},
        {"Mirror", "shape", "", "symmetry symmetric flip half"},
        {"RestoreLength", "shape", "", "preserve length stretch"},
        {"Sculpt", "shape", "", "brush correctives strokes edit guides"},
        // Deform
        {"Bend", "deform", "", "comb curve lift lay"},
        {"Transform", "deform", "", "move rotate scale translate"},
        {"Tie", "deform", "", "ponytail ring hair tie bun"},
        {"BlendNeighbors", "deform", "", "coherent flow neighbours"},
        {"Gravity", "deform", "", "weight droop sag fall"},
        {"Displace", "deform", "", "volume push offset normal"},
        // Collide
        {"Collide", "collide", "", "collision penetration body scalp"},
        {"SelfCollide", "collide", "", "self collision layers packed intersect"},
        {"MeshCut", "collide", "", "cut hull obstacle trim"},
        // Look
        {"Clump", "look", "", "clumps groups locks"},
        {"Twist", "look", "", "rope twisted locks"},
        {"Braid", "look", "", "plait cornrow fishtail rope"},
        {"Noise", "look", "", "frizz random jitter"},
        {"Curl", "look", "", "coil wave helix kinky afro"},
        {"Flyaway", "look", "", "strays frizz loose"},
        {"Static", "look", "", "charged puff repel"},
        {"Tangle", "look", "", "mats matted stuck"},
        {"Wet", "look", "", "water damp clumps"},
        {"Instance", "look", "", "copy mesh leaves beads"},
        {"Scales", "look", "", "reptile armor plates skin"},
        // Motion
        {"Wind", "motion", "", "breeze air turbulence"},
        {"Jiggle", "motion", "", "secondary lag overshoot follow"},
        {"Simulate", "motion", "", "simulation dynamics physics"},
        // Feathers
        {"Propagate", "feathers", "", "barbs fern feather"},
        {"Plume", "feathers", "", "feather layers overlap"},
        {"Split", "feathers", "", "outputs share designs"},
        // Color
        {"HairColor", "color", "", "melanin pigment dye shading specular roughness"},
        {"Bands", "color", "", "agouti ticked stripes"},
        {"FeatherColor", "color", "", "pattern bars spots iridescence"},
        {"RenderData", "color", "", "opacity shader"},
        // Convert
        {"ToGuides", "convert", "", "strands to guides"},
        {"ToStrands", "convert", "", "guides to strands"},
        {"StrandMesh", "convert", "", "polygon tubes ribbons geometry"},
        // Fields & Curves
        {"Map", "field", "", "paint texture image weight"},
        {"Expression", "field", "", "formula code script"},
        {"Control", "field", "", "slider channel box key"},
        {"FieldNoise", "field", "", "noise texture variation"},
        {"FieldRemap", "field", "", "remap curve ramp"},
        {"FieldCombine", "field", "", "multiply add mix"},
        {"Curve", "field", "", "ramp shared"},
        // Utility
        {"Value", "utility", "", "number constant parameter"},
        {"Group", "utility", "", "region mask selection set"},
        {"Blend", "utility", "", "mix looks wet dry"},
        {"Attribute", "utility", "", "channel primvar data"},
        {"Merge", "utility", "", "combine join"},
        {"Switch", "utility", "", "variant choose pick"},
        {"Output", "utility", "", "result render final"},
        {"Compound", "utility", "", "group subgraph", true},
        // Games
        {"Cards", "games", "", "game unreal unity cards lod"},
    };
    return entries;
}

const NodeCatalogEntry* nodeCatalogEntry(const std::string& type) {
    static const std::map<std::string, const NodeCatalogEntry*> byType = [] {
        std::map<std::string, const NodeCatalogEntry*> out;
        for (const NodeCatalogEntry& e : nodeCatalog()) out[e.type] = &e;
        return out;
    }();
    const auto it = byType.find(type);
    return it == byType.end() ? nullptr : it->second;
}

std::string nodeLabel(const std::string& type) {
    const NodeCatalogEntry* entry = nodeCatalogEntry(type);
    if (entry && !entry->label.empty()) return entry->label;
    std::string out;
    for (std::size_t i = 0; i < type.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(type[i]);
        const bool lowerBefore = i > 0 && std::islower(static_cast<unsigned char>(type[i - 1]));
        if (std::isupper(c) && lowerBefore) out += ' ';
        out += static_cast<char>(c);
    }
    return out;
}

}  // namespace groomlab

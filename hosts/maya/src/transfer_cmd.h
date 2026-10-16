// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <maya/MDGModifier.h>
#include <maya/MDagPath.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

#include "groomlab/transfer.h"

namespace groomlab {
class ScalpMesh;
struct ScalpMaps;
}

namespace groomlab::maya {

// groomLabTransfer: moves a groom to another scalp mesh (GH-8), one undo step.
// Guides, parts and flow arrows are re-rooted on the new scalp (at the current frame) where the
// transfer method takes them (transfer.h ScalpMapping); attribute layers ride with the guides.
// The groom's inMesh is reconnected to the new mesh and its rest mesh disconnected.
//
//   -scalp/-sc <mesh>  the new scalp (required)
//   -method/-mth <m>   closest (default), bounds, markersAlign, markersWarp or uv (transfer.h)
//   -fitBounds/-fb     the bounds method (when -method is not given): first move and scale the
//                      old scalp's bounding box onto the new one's
//   -markers/-mk <json>  the marker pairs (transfer.h transferMarkersToJson; a side may give a
//                      point alone): snapped onto the meshes and stored in the groom's
//                      transferMarkers (undoably). Without it a marker method reads the groom's
//                      transferMarkers. Fallbacks (a warp with too few pairs aligns) are warned
//                      about.
//   -keepLengths/-kl   guides keep their lengths (they turn with the head, they do not scale)
//   -snapMarkers/-snm  with -scalp and -markers: changes nothing, returns the markers snapped
//                      (old sides on the groom's scalp, new sides on -scalp) as JSON
//   -maps/-mps         also project the painted maps of scalpMaps onto the new scalp
//                      (scalp_paint.h: projectVertexMap, projectTexture): a vertex map gets
//                      values for the new scalp's vertices, a texture map a new image at the
//                      new scalp's UVs, written under the groom's masks folder with a free name
//                      (the old file is never written: other grooms or the old scene may read
//                      it) and the map repointed. Channels are kept. A map that cannot be
//                      projected (its values do not fit the old scalp, its image is missing, a
//                      scalp without UVs) stays as it was, with a warning. Off by default: the
//                      Python API (transfer_groom) passes it.
//
// The groom is the command's object (or the selection). Returns the number of
// guides moved.
//
// Maps only, from another groom (Copy Graph's maps across scalps):
//   -mapsFrom/-mfr <groom> [-mapName/-mn <name> ...] [-fitBounds | -method closest|bounds|uv]
// projects the other groom's maps (the ones named, else all) from its scalp onto this groom's
// scalp, adding or replacing them in this groom's scalpMaps; nothing else changes. Returns the
// names projected (skipped ones are warned about).
//
// Undo restores the attributes and connections and removes the image files the command wrote;
// redo writes them again (the same bytes).
class TransferCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    static MSyntax newSyntax();

    MStatus doIt(const MArgList& args) override;
    MStatus redoIt() override;
    MStatus undoIt() override;
    bool isUndoable() const override { return undoable_; }

private:
    // Projects the maps of `maps` (those in `only`, every one when it is empty) from `from` onto
    // `to`: vertex maps' values replaced, texture maps' images written for `groom` (files in
    // written_) and repointed. Texture files resolve against `sourceNode`. Returns the names
    // projected; each map that could not be is warned about and left as it was.
    std::vector<std::string> projectMaps(ScalpMaps& maps, const std::set<std::string>& only, const ScalpMapping& mapping,
                                         const MObject& sourceNode, const MDagPath& groom);
    MStatus mapsFromOtherGroom(const MDagPath& groom, const std::string& source, const std::set<std::string>& only,
                               TransferMethod method);

    MDGModifier modifier_;
    bool undoable_ = false;
    struct WrittenFile {
        std::string path;
        std::vector<std::uint8_t> bytes;
    };
    std::vector<WrittenFile> written_;  // image files the command made (removed on undo)
};

// groomLabCopyColorSets <mesh> -source/-src <mesh> -colorSet/-cs <name> [-cs ...]: the named
// color sets of -source (the same topology: a copy of <mesh>) made on <mesh>, color for color
// per face-vertex, as data of the mesh (no history). A set <mesh> already has is skipped. One
// undo step, undone exactly: the sets made are deleted and the current set put back (a
// transferAttributes baked into a mesh without history does not undo cleanly in Maya).
// Returns the names made.
class CopyColorSetsCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    static MSyntax newSyntax();

    MStatus doIt(const MArgList& args) override;
    MStatus redoIt() override;
    MStatus undoIt() override;
    bool isUndoable() const override { return true; }

private:
    struct ColorSet;
    MDagPath target_;
    std::vector<std::shared_ptr<ColorSet>> sets_;
    std::string current_;  // the target's current color set before
};

}  // namespace groomlab::maya

// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

namespace groomlab::maya {

// `groomLabSimulate <groom>`: the groom's Simulate nodes (the core's simulation, simulation.h),
// stepped in the groom's own session (the viewport's).
//   -toFrame N    Simulate Range: every Simulate node stepped on to frame N from its last frame
//                 (the frames on the way read the scalp from inMesh at their times); returns the
//                 frames stepped.
//   -invalidate   drops the frames in memory (-node: one node's); bakes stay (simCaches).
//   -status       JSON: [{node, startFrame, ranges, frames, live, baked, bakeStale, bakedFirst,
//                 bakedLast, bakedFile, stepMs, bytes, stamp}].
//   -nodes        the Simulate nodes making the output, upstream first.
//   -bake file -node n [-startFrame a] [-endFrame b]: writes n's frames a..b (its start frame to its last
//                 simulated frame by default) as an Alembic guide cache (sim_cache.h); returns
//                 the settings' stamp to store with it (groomlab.sim.bake_simulation stores both
//                 in simCaches, undoably). Frames not simulated are an error: -toFrame first.
//   -stamp -node n  the stamp a bake of n would record now.
// Not undoable: the frames are a cache, not scene data.
class SimulateCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    static MSyntax newSyntax();
    MStatus doIt(const MArgList& args) override;
};

}  // namespace groomlab::maya

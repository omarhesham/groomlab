// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MDGModifier.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

namespace groomlab::maya {

// groomLabParts: edits a groom's parts (stored in its partData attribute,
// bound to the scalp). One action per call; every edit is undoable.
//
//   -fromCurves/-fc <curve> (multi-use) [-name n]   adds one part per curve (projected onto the scalp)
//   -remove/-rm <name>                              removes a part
//   -clear/-cl                                      removes every part
//   -list/-ls                                       returns the part names
//
// The groom is the command's object (or the selection).
class PartsCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    static MSyntax newSyntax();

    MStatus doIt(const MArgList& args) override;
    MStatus redoIt() override { return modifier_.doIt(); }
    MStatus undoIt() override { return modifier_.undoIt(); }
    bool isUndoable() const override { return undoable_; }

private:
    MDGModifier modifier_;
    bool undoable_ = false;
};

}  // namespace groomlab::maya

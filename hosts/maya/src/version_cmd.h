// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

namespace groomlab::maya {

// `groomLabBakeCards <groom> -directory d [-name base] [-resolution 2048] [-noStore]`:
// bakes the groom's cards into TGA maps; returns the paths. base defaults to the
// groom's asset name; -noStore leaves the groom's cardMaps alone (exports). -layout unity
// writes Unity's PNG maps instead (GroomShape::bakeCards; an export, never stored).
class BakeCardsCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    static MSyntax newSyntax();
    MStatus doIt(const MArgList& args) override;
};

// `groomLabGraphStats <groom>`: JSON {"view": node, "times": {node: ms}} from
// the groom's last evaluation, for the graph editor's timing tags.
class GraphStatsCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    // -points: the strands the viewport shows (its evaluation at the preview density), as a flat
    // x y z list, instead of the stats JSON: what scripts and tests compare exports with.
    // -drawTimes (report-only): {"computeMs": the groom's last evaluation in compute, "draw": the
    // last viewport update that uploaded geometry, timed (lastDrawTimesJson)}.
    static MSyntax newSyntax();
    MStatus doIt(const MArgList& args) override;
};

// `groomLabVersion`: returns the core library version. Confirms the plugin and
// core are linked and loaded.
class VersionCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

// `groomLabDefaultGraph`: returns the default graph as JSON, the graph a
// groom uses while its `graph` attribute is empty or "{}".
class DefaultGraphCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

// `groomLabOperators`: every registered operator (ports, params, defaults) as
// JSON, see groomlab::operatorSpecsJson. Used by the graph editor.
class OperatorsCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

// groomLabExpression -check/-c <text> [-variables/-v <json>]: compiles an expression (with an
// Expression node's own variables) and returns "" or what is wrong with it, for the editor to
// show while you type, connected or not.
class ExpressionCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    static MSyntax newSyntax();
    MStatus doIt(const MArgList& args) override;
};

// `groomLabPaths`: re-reads the Maya project and scene folder that relative
// paths resolve against, and returns those folders. `-resolve/-r path`
// returns a path resolved against them instead (with `-node/-nd n`, as node n
// resolves it: from a referenced file, that file's folder first).
// `-assetName/-an groom` returns the groom's asset name (its files' names);
// `-assetFolder/-af kind` the scene's folder for that kind of file.
class PathsCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    static MSyntax newSyntax();
    MStatus doIt(const MArgList& args) override;
};

}  // namespace groomlab::maya

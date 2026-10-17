// SPDX-License-Identifier: Apache-2.0
#pragma once

// The groom the app has open: a groom file (its scalp and caches through GroomScene), the
// session that keeps it evaluated, the graph as JSON the panels edit (every param set goes to
// the session as a whole graph, which adopts its cache, so only the edited node and what follows
// re-run), an undo history of those edits, the display builder, the time it is evaluated at
// (the timeline's frame; followScalp), saving, the card bake, the simulation (its cache and bake)
// and every export (Alembic curves, USD, Unreal's groom, Unity's strands, cards as glTF:
// groom_exports.cpp). No Qt: the tests
// drive it without a window, and a later host (a CLI batch, a script) could too.

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "groomlab/card_files.h"
#include "groomlab/cards.h"
#include "groomlab/display.h"
#include "groomlab/frame_time.h"
#include "groomlab/groom_file.h"
#include "groomlab/groom_scene.h"
#include "groomlab/groom_session.h"
#include "groomlab/guide_hierarchy.h"
#include "groomlab/scalp_maps.h"
#include "groomlab/scalp_paint.h"
#include "groomlab/simulation.h"
#include "groomlab/stroke.h"

namespace groomlab::app {

// A painted map of the groom as the panels list it: a vertex map or a texture map (scalp_maps.h,
// the session's ScalpMaps) or a guide layer (an attribute of the authored hierarchy).
struct PaintedMap {
    std::string name;
    std::string kind;     // "vertices", "texture" or "guides"
    int channels = 1;     // 1 gray, 4 color (a guide layer is gray)
    std::string texture;  // a texture map's file as stored (relative to the groom file)
};

// The frames an export covers (frames at the document's framesPerSecond, Maya's convention:
// frame 1 at 1/24 s; `step` 1 every frame).
struct FrameRange {
    double first = 1.0, last = 1.0, step = 1.0;
    std::size_t count() const;  // frames from first to last by step (at least 1)
    double frame(std::size_t index) const { return first + static_cast<double>(index) * step; }
};

struct ExportOptions {
    FrameRange frames;
    float density = 1.0f;  // the share of the strands written (1: every strand)
    // Unreal's groom and Unity's strands, as the Maya host's export writes them (engine_export.h):
    // each groom group (the groupId channel) its own curves object, and (Unreal) the primary
    // guides as the guides Unreal simulates.
    bool groups = true;
    bool guides = true;
    // Feathers: the shafts as strands (unset: left out when the feathers have a rachis mesh), a
    // feather group's guides its shafts (Unreal), a lighter groom (barbs kept at barbDensity,
    // each barbWidthScale as wide), the feather groups' points per strand (Unity; 0: as the rest).
    std::optional<bool> shafts;
    bool featherGuides = false;
    bool iridescenceAo = false;  // Unreal: FeatherColor's iridescence as groom_ao too
    float barbDensity = 1.0f;
    float barbWidthScale = 1.0f;
    int barbPoints = 0;
    // Unity's strands: the scale (1 keeps the scene's centimeters, what its importer's default
    // settings expect) and the points per strand (0: as they are; 3 to 128 lets Unity leave
    // "Resample Curves" off).
    float scale = 1.0f;
    int pointsPerStrand = 0;
    // Cards: the atlas resolution (0: the Cards node's atlasSize), the ambient occlusion
    // strength, and a binary .glb (else a .gltf with its .bin and PNGs beside it).
    int resolution = 0;
    float occlusion = 1.0f;
    bool binary = true;
};

// After each frame: the frames done of the count; false stops the export (what was written is
// closed as it is: a shorter cache).
using ExportProgress = std::function<bool(std::size_t done, std::size_t count)>;

struct ExportResult {
    std::vector<std::string> files;  // the main file first
    std::size_t frames = 0;
    std::size_t strands = 0;  // of the last frame written
    bool cancelled = false;
};

// A bake written beside the groom (bakeCards): the maps as files (card_files.h's layout, the
// Maya host's), the atlas, the stamp of what they were made from. (A bake in memory is the
// core's CardBake.)
struct CardBakeFiles {
    std::string prefix;              // <folder>/<name>_ (CardBakePaths)
    std::vector<std::string> paths;  // _Depth, _Coverage, _Tangent, _Attribute, _Color, _AO (TGA), _Atlas.json, _Preview.png
    std::string preview;             // the viewport's RGBA map: the color darkened by half the AO, the coverage in alpha
    std::string atlas;               // the atlas JSON the session keeps cells by
    std::string stamp;               // cardsBakeStamp of the bake
    int size = 0;
    std::size_t cards = 0, triangles = 0, textures = 0;
};

class GroomDocument {
public:
    // Reads the file, opens its caches, makes the session. Throws std::runtime_error (a
    // GroomFileError, AlembicError or GraphError's message).
    static std::unique_ptr<GroomDocument> open(const std::string& path);
    // A new groom on a mesh with the default graph and no guides; `meshPath` is remembered as
    // the source. Throws std::runtime_error for a scalp without triangles.
    static std::unique_ptr<GroomDocument> newOnMesh(MeshPtr scalp, const std::string& meshPath);
    // A new groom on an Alembic mesh: its first frame's mesh as the scalp and, when the mesh
    // animates, the file as the scalp cache, so the groom follows it frame by frame (and the
    // timeline spans its samples). `object` names the mesh in the file ("" the first). Throws
    // std::runtime_error (an AlembicError's message).
    static std::unique_ptr<GroomDocument> newOnAlembic(const std::string& abcPath, const std::string& object = {});
    ~GroomDocument();
    GroomDocument(const GroomDocument&) = delete;
    GroomDocument& operator=(const GroomDocument&) = delete;

    const std::string& path() const { return path_; }  // "" until saved
    const GroomFile& file() const { return scene_->file(); }
    GroomSession& session() { return scene_->session(); }
    const GroomSession& session() const { return scene_->session(); }
    GroomScene& scene() { return *scene_; }
    bool dirty() const { return dirty_; }

    // ---- Time --------------------------------------------------------------------------------
    double framesPerSecond() const;
    // Another time can give other strands: a scalp or guide cache, or a graph that reads the
    // time (Wind, Jiggle, ImportCurves, a fading corrective). The timeline shows then.
    bool animated() const { return scene_->readsTime(); }
    // The frames the caches span (first and last, at framesPerSecond); false when no cache
    // animates (a graph reading the time alone has no range of its own: the host picks one).
    bool cacheFrameRange(double* first, double* last) const;
    // The time the next evaluation (and export without a range) is at.
    double time() const { return time_; }  // seconds
    double frame() const { return secondsToFrame(time_, framesPerSecond()); }  // every host's rule (frame_time.h)
    void setTime(double seconds) { time_ = seconds; }
    void setFrame(double frame) { time_ = frameToSeconds(frame, framesPerSecond()); }
    // Following the scalp (GroomSession::setFollowScalp): kept in the file ("followScalp").
    bool followScalp() const { return session().followScalp(); }
    void setFollowScalp(bool on);
    // Scaling with the scalp (GroomSession::setScaleSource; the file's "scaleSource" and
    // "scaleReference"): what the groom's scale follows, and Set Scale Reference (the scalp's size
    // now is 1x).
    void setScaleSource(ScaleSource source);
    void setScaleReference();

    // ---- The graph ---------------------------------------------------------------------------
    struct NodeInfo {
        std::string name, type;
        bool bypass = false;
        std::string evaluation = "always";  // when it runs: always, viewportOnly, renderOnly
    };
    // The nodes as the JSON lists them (the default graph's when the file said {}).
    std::vector<NodeInfo> nodes() const;
    struct ConnectionInfo {
        std::string from, output, to, input;  // output "": the source's first
    };
    std::vector<ConnectionInfo> connections() const;
    const nlohmann::json& graph() const { return graph_; }
    // Where the node graph draws a node (its "ui": {"x", "y"} in the graph JSON, graph_io.h, kept in the
    // file, Maya's Graph Editor's too; false when the node has none yet: the view lays it out).
    bool nodePosition(const std::string& node, float* x, float* y) const;
    // Not an undo step (as Maya's node editor moves are not); the file is dirty after it.
    void setNodePosition(const std::string& node, float x, float y);
    std::string graphJson() const { return graph_.dump(); }
    // A node's param as stored (null when the node or param is absent: the spec's default then).
    nlohmann::json param(const std::string& node, const std::string& name) const;
    // A param's value for the panel: the stored value, else the spec's default, else null.
    nlohmann::json paramOrDefault(const std::string& node, const std::string& name) const;
    // The operator specs by type (graph_io.h operatorSpecsJson): what a node's params are,
    // their kinds, defaults, choices, descriptions. Null for an unknown type.
    const nlohmann::json* spec(const std::string& type) const;
    const nlohmann::json* nodeSpec(const std::string& node) const;
    std::vector<std::string> operatorTypes() const;  // every type with a spec, sorted
    // A mappable param fed by a field: the node whose output feeds the param's input port (the
    // param exposed in the node's "paramInputs" and a connection to it), "" when nothing does.
    std::string paramInputSource(const std::string& node, const std::string& name) const;
    bool paramExposed(const std::string& node, const std::string& name) const;
    // Exposes the param as a Field (or Curve) input port the graph can wire, or hides the port
    // again (what fed it is disconnected). One undo step; "" or the error.
    std::string exposeParam(const std::string& node, const std::string& name, bool exposed);
    // A map for this param (+ New Map > for a setting; a param's Paint, as Maya's right-click >
    // Paint): the map the param reads already, else a map named <node>_<name> (made free) of
    // `kind` ("vertices" or "texture"), filled 1.0, and the param reading it (0 to its value
    // now, so the look stays as it is until painted) as one undo step (*made: true). The map's
    // name is written to *mapName. "" or the error.
    std::string paintParam(const std::string& node, const std::string& name, std::string* mapName = nullptr,
                           const std::string& kind = "vertices", bool* made = nullptr);
    // The scalp's color sets that are not painted maps (Maya's, read by name).
    std::vector<std::string> colorSetNames() const;
    // The strands' per-strand channels of the last view (what the attribute color mode shows).
    std::vector<std::string> strandChannelNames() const;

    // ---- Edits (each one undo step) ----------------------------------------------------------
    // Sets the param and hands the graph to the session. Returns "" or why the graph refused it
    // (the edit is taken back then). `record` false: a live drag's steps, not in the history
    // (the drag's end records one edit from where it started).
    std::string setParam(const std::string& node, const std::string& name, const nlohmann::json& value,
                         bool record = true);
    // Records an edit made with record false: the value before the drag and the one it ended at.
    void recordEdit(const std::string& node, const std::string& name, const nlohmann::json& before,
                    const nlohmann::json& after);
    // A live drag previewed elsewhere (the GPU look preview, gpu_preview.h): while this answers
    // true for a step of a drag (setParam with record false, a number or a ramp's keys), the value
    // is held here (param and paramOrDefault show it) and the graph is not touched, so nothing
    // evaluates; the drag's end (recordEdit, or a recorded setParam of that param) applies the
    // held value for real, after telling `livePreviewEnded`. Answering false previews on the CPU
    // as ever.
    std::function<bool(const std::string& node, const std::string& name, const nlohmann::json& value)> livePreview;
    std::function<void()> livePreviewEnded;
    bool previewing() const { return preview_.held; }
    std::string setBypass(const std::string& node, bool bypass);
    // When the node runs (graph.h EvaluationMode): "always", "viewportOnly" (exports skip it) or
    // "renderOnly" (the viewport skips it). One undo step. "" or why it was refused.
    std::string setEvaluation(const std::string& node, const std::string& mode);
    // The graph's structure edited by the node graph view: `edit` runs on a copy of the JSON,
    // which the session takes; one undo step named `label` holding the whole graph before and
    // after (graphs are small). "" or why the session refused it (nothing changes then). Sculpt
    // nodes' edits (sculpt.h) follow in the same step: a renamed node's (`renamed`: old -> new),
    // a copy's (`copied`: copy -> source), and a node gone loses its (the sculpt target too).
    std::string editGraph(const std::string& label, const std::function<void(nlohmann::json&)>& edit,
                          const std::map<std::string, std::string>& renamed = {},
                          const std::map<std::string, std::string>& copied = {});
    // A node of `type` at a position, named after its type (made unique) unless `name` says;
    // the name made is written to *made. The connections of its strands input and output are
    // the caller's (connectNodes).
    std::string addNode(const std::string& type, float x, float y, std::string* made, const std::string& name = {});
    // Removes the node with every connection to or from it.
    std::string removeNode(const std::string& node);
    // Connects `from`'s output (`output` "": its first) to `to`'s `input`, replacing what fed it:
    // the user's link (GraphEdit::connectByHand: a Clump fed centers takes them).
    std::string connectNodes(const std::string& from, const std::string& output, const std::string& to, const std::string& input);
    std::string disconnectInput(const std::string& to, const std::string& input);
    // Another graph altogether (a preset's JSON): the history is dropped. "" or the error (the
    // graph as it was stays then).
    std::string replaceGraph(const std::string& graphJson);
    // The node editors' other edits (GraphEdit's), each one undo step: a rename (the connections
    // and the output follow), a copy of a node beside it (its params too; the name made is
    // written to *made), the graph's output, and every position dropped so the view lays the
    // graph out afresh (Tidy Layout). "" or the error.
    std::string renameNode(const std::string& node, const std::string& newName);
    std::string duplicateNode(const std::string& node, std::string* made);
    std::string setOutputNode(const std::string& node);
    std::string tidyLayout();
    // A node's behaviour version as saved and its operator's newest (graph_edit.h): Upgrade is on
    // offer while the first is lower. {0, 0} for no such node.
    std::pair<int, int> nodeVersion(const std::string& node) const;
    // The node takes its operator's newest behaviour (GraphEdit::upgradeNode), one undo step. "" or
    // the error (one for a node at the newest already).
    std::string upgradeNode(const std::string& node);
    // The time each node took in its last run, by name (ms): the graph's badges.
    std::vector<std::pair<std::string, double>> nodeTimes() const { return scene_->nodeTimes(); }
    // Each node's note from the last evaluation (EvalContext::note: it does nothing as set), an
    // Instance's whose prototype mesh the groom file does not hold, and a node's reading an attribute
    // no node upstream writes (channels(); not a read of an optional default, as Clump's region).
    std::vector<std::pair<std::string, std::string>> nodeNotes() const;
    // The attributes (strand channels) per node as the graph is set, without evaluating it
    // (graphChannelsJson, attributes.h; the guide layers as the guides' painted maps): what reaches
    // each node and from which node, what it writes and overwrites, whether its reads reach it.
    nlohmann::json channels() const;
    bool canUndo() const { return undoIndex_ > 0; }
    bool canRedo() const { return undoIndex_ < edits_.size(); }
    std::string undoText() const;  // "Undo length.length", "Undo comb stroke" or ""
    std::string redoText() const;
    std::string undo();  // "" or the error
    std::string redo();

    // ---- Guides: strokes and edits (each one undo step) --------------------------------------
    // A stroke's edit from the core's tools (the stored texts after it): applied to the session
    // (the texts before recorded here, so undo puts them back bit for bit) and recorded as one
    // step named after its mode ("comb stroke"). An empty edit records nothing.
    void commitStroke(StrokeEdit edit);
    // An edit of the guides as stored: `edit` runs on a copy of the authored hierarchy (rooted on
    // the scalp), which is evaluated on it again (new children take their shapes) and stored as
    // one step named `label` ("delete guides"). "" or why the edit threw (nothing recorded then).
    std::string editGuides(const std::string& label, const std::function<void(GuideHierarchy&)>& edit);
    // An edit of a stored text the strokes also edit (the parts, the flow, the correctives:
    // StrokeField), as one undo step named `label`: `edit` runs on the text as stored, which the
    // session takes back when it changed. "" or why the edit threw (nothing recorded then).
    std::string editStoredText(const std::string& label, StrokeField field, const std::function<void(std::string&)>& edit);
    std::string storedText(StrokeField field) const;  // as the strokes read it (SessionStrokeHost)
    // Sculpt nodes (sculpt.h): the graph's (top level, in order); the one brushes shape ("" none:
    // the guides or a corrective; the session's sculpt target: its output is drawn as the guides);
    // a node's edits dropped (one undo step); how many guides of a node hold an edit.
    std::vector<std::string> sculptNodes() const;
    void setSculptTarget(const std::string& node);
    const std::string& sculptTarget() const;
    std::string clearSculptEdits(const std::string& node);
    std::size_t sculptedGuides(const std::string& node) const;
    // The groom moved onto another scalp (GH-8, the core's transfer): every guide re-rooted where
    // the transfer method takes it (transfer.h: closest, bounds, markersAlign, markersWarp, uv;
    // keepLengths), the parts and flow with them, the painted vertex and texture maps projected
    // through the same mapping when `maps` (a texture map's new image goes beside the groom,
    // which needs it saved); the scalp and guide caches are dropped. A marker method takes
    // options.markers, else the groom's stored ones (GroomFile::transferMarkers); the markers used
    // are stored with the groom (snapped onto the two meshes), so saving keeps them. The history
    // is dropped (the scalp itself is replaced). What was dropped on the way, and fallbacks (a
    // warp with too few markers aligned instead, roots without a UV match), are said in
    // *warnings. "" or the error.
    std::string transferToScalp(MeshPtr scalp, const TransferOptions& options, bool maps,
                                std::vector<std::string>* warnings = nullptr);
    // The same by closest point, or bounds with `fit`.
    std::string transferToScalp(MeshPtr scalp, bool fit, bool maps, std::vector<std::string>* warnings = nullptr);
    // The marker pairs stored with the groom (its file's "transferMarkers"): what a marker transfer
    // uses by default, saved with the groom. setTransferMarkers replaces them (the Transfer window
    // stores its complete pairs on every edit): no history step (the window has its own), dirty.
    const std::vector<TransferMarker>& transferMarkers() const;
    void setTransferMarkers(std::vector<TransferMarker> markers);
    // The Reference window's photos (GroomFile::referencePhotos: a JSON array as text, "" none):
    // saved with the groom. setReferencePhotos replaces them (the window stores on every edit): no
    // history step (the window keeps its own per photo), dirty.
    const std::string& referencePhotos() const;
    void setReferencePhotos(std::string json);

    // The guide selection: highlighted in the display; what brushes with selectedOnly act on.
    // Not in the history (as Maya's selection is not).
    const std::vector<GuideId>& selection() const { return selection_; }
    void setSelection(std::vector<GuideId> ids);
    std::uint64_t selectionVersion() const { return selectionVersion_; }
    // Guide groups hidden in the viewport (not drawn, not brushed), by name.
    const std::set<std::string>& hiddenGroups() const { return hiddenGroups_; }
    void setGroupHidden(const std::string& name, bool hidden);
    std::string hiddenGroupsText() const;  // DisplaySettings::hiddenGuideGroups' form

    // ---- Painted maps (each edit one undo step) ------------------------------------------------
    // The groom's maps: the scalp maps the session holds (vertex and texture maps, which the
    // file keeps as the scalp's color sets and its map catalog) and the guide layers.
    std::vector<PaintedMap> maps() const;
    const PaintedMap* map(const std::string& name) const;  // null when there is none (valid until the next maps())
    // A new map filled with `value` (a color map with `color`): a vertex map, a texture map (a
    // PNG of `resolution` square under the groom's masks folder, mapsDirectory(), which needs
    // the groom saved) or a guide layer (every primary guide, children inheriting). "" or why not.
    std::string createMap(const std::string& name, const std::string& kind, bool color, float value, int resolution = 1024,
                          const PaintColor& fill = {0.0f, 0.0f, 0.0f, 1.0f});
    // Renames a map; the Map nodes and map params reading it follow, as does a texture map's
    // file when it is named after the map. "" or why not.
    std::string renameMap(const std::string& from, const std::string& to);
    // Removes a map (a texture map's file stays on disk). "" or why not.
    std::string removeMap(const std::string& name);
    // Where a texture map's file goes: <groom folder>/masks/<groom name>/ ("" until the groom is
    // saved), and the file of a texture map resolved against the groom's folder.
    std::string mapsDirectory() const;
    std::string textureMapFile(const std::string& name) const;
    // The texture map's paint for a stroke (StrokeHost::texturePaint): opened from its file, or
    // blank at `resolution` when the file is not there yet, and kept while the file is what the
    // strokes wrote. Throws std::runtime_error for a map that is not a texture map.
    std::shared_ptr<TexturePaint> texturePaint(const std::string& name, int channels, int resolution);
    void dropTexturePaint() { paints_.clear(); }
    // The map shown on the scalp (DisplayInputs::scalpMap) by name ("" none), `channel` rgb or
    // r g b a: a vertex map or guide layer as the session views it; a texture map (its file, or
    // the brush's live image while painting) sampled at the scalp's vertices, since the viewport
    // draws the map per vertex.
    ScalpMapView scalpMapView(const std::string& name, const std::string& channel);
    // The map updateDisplay shows on the scalp ("" none; as the Maya plugin's scalpMapDisplay).
    void setScalpMapDisplay(const std::string& name, const std::string& channel = "rgb");
    const std::string& scalpMapDisplay() const { return scalpMapName_; }
    const std::string& scalpMapChannel() const { return scalpMapChannel_; }

    // ---- Evaluation and display --------------------------------------------------------------
    // The view at the document's time (the request's own time is replaced by it).
    const ViewResult& evaluate(const ViewRequest& request) {
        ViewRequest timed = request;
        timed.time = time_;
        return scene_->view(timed);
    }
    const ViewResult& lastView() const { return scene_->session().last(); }
    // The display frame of the last view with these settings (DisplayBuilder::update), the
    // selection highlighted, the strands stepping back while a stroke is live (the session has a
    // preview: DisplayInputs::stroke).
    const DisplayFrame& updateDisplay(const DisplaySettings& settings);
    DisplayBuilder& display() { return display_; }
    // The last view's bounds with the scalp's (what frame-all fits).
    Bounds sceneBounds() const;

    // ---- Files -------------------------------------------------------------------------------
    // Writes the groom file with the current graph and the guides, parts, flow and correctives
    // as the session holds them (strokes and guide edits included); throws std::runtime_error.
    void save(const std::string& path);
    // The groom as save writes it (the graph, the session's guides and overlays, the maps as
    // color sets of the scalp): what a save, or another scene made from this one, starts from.
    GroomFile currentFile() const;
    // The output at full density (no point budget) as Alembic curves at the document's time, root
    // UVs from the scalp; throws std::runtime_error. Returns the strands written.
    std::size_t exportAlembic(const std::string& path);

    // ---- Exports (groom_exports.cpp) ---------------------------------------------------------
    // Each writes the frames of `options.frames` (the groom evaluated at each, at `density`:
    // full density on the session's own document, so the view's cache stays) and returns the
    // files written; throws std::runtime_error with the writer's message. `progress` is called
    // after each frame. The document's time is left as it was.
    // Alembic curves (AlembicCurvesWriter: strandId, the channels, Cd, root UVs).
    ExportResult exportAlembic(const std::string& path, const ExportOptions& options, const ExportProgress& progress = {});
    // USD BasisCurves in a text layer (.usda).
    ExportResult exportUsd(const std::string& path, const ExportOptions& options, const ExportProgress& progress = {});
    // Unreal's groom importer (UnrealGroomWriter): the strands with the primary guides, by
    // groom group when `options.groups`.
    ExportResult exportUnrealGroom(const std::string& path, const ExportOptions& options, const ExportProgress& progress = {});
    // Unity's hair package (com.unity.demoteam.hair): curves with per-point root UVs and
    // positive root widths, by groom group of at most 64,000 strands.
    ExportResult exportUnityStrands(const std::string& path, const ExportOptions& options, const ExportProgress& progress = {});
    // Game cards as glTF (.glb, or .gltf with its files beside it): the cards baked at the
    // document's time with their maps embedded (<name>_BaseColor: color and coverage, <name>_ORM:
    // occlusion), and the hair cap when the scalp has UVs. One frame (cards are static).
    ExportResult exportCards(const std::string& path, const ExportOptions& options, const ExportProgress& progress = {});

    // ---- Cards (groom_exports.cpp) -----------------------------------------------------------
    // The Cards nodes of the graph, by name (the panel's choices).
    std::vector<std::string> cardsNodes() const;
    // Adds a Cards node between what feeds the output and the output (one undo step); the name
    // made is written to *made. "" or the error.
    std::string addCardsNode(std::string* made);
    // Sets a Cards node for a level of detail ("hero", "mid", "low": the core's presets,
    // card_presets.h, as Maya's cards_preset sets them, by what the node cards), one undo step.
    // "" or the error.
    std::string applyCardPreset(const std::string& node, const std::string& lod);
    // Sets a Simulate node to a preset ("default", "fur": the core's simulatePresets, as Maya's
    // simulate_preset sets them), one undo step. "" or the error.
    std::string applySimulatePreset(const std::string& node, const std::string& preset);
    // The cards baked in memory at the document's time (the Cards nodes building their cards at
    // full density, on the session's card document). Throws std::runtime_error without a Cards
    // node or strands.
    CardBake bakeCardsInMemory(int resolution = 0, float occlusion = 1.0f);
    // Writes the bake as files: <directory>/<name>_<map>.tga for Depth, Coverage, Tangent,
    // Attribute, Color and AO (the Maya host's layout and names), <name>_Atlas.json (the cells
    // rebuilt cards keep) and <name>_Preview.png (the viewport's RGBA map). The session takes
    // the bake (setCardMaps): the cards keep its cells from now on and the stats say when they
    // went stale. Throws std::runtime_error.
    CardBakeFiles bakeCards(const std::string& directory, const std::string& name, int resolution = 0, float occlusion = 1.0f);
    // The last bake written or adopted (null: none). Opening a groom adopts the bake its file
    // names ("cardBake", with its stamp; saving names the last bake), else a bake beside it at
    // the default directory and name.
    const CardBakeFiles* lastBake() const { return bake_ ? &*bake_ : nullptr; }
    // Where a bake goes by default: "<groom folder>/<stem>_cards" and the stem (an unsaved groom:
    // the system's temp folder and "groom").
    std::string defaultBakeDirectory() const;
    std::string defaultBakeName() const;
    // The last view's cards changed since the last bake (GroomStats::cardsStale).
    bool bakeStale() const;

    // ---- Simulation (groom_exports.cpp; the core's simulation.h) ------------------------------
    // The Simulate nodes making the output, upstream first: the timeline's sim controls show while
    // there is one. Playing or scrubbing steps a node in cacheMode "live" as the view asks for
    // frames (GroomScene feeds the scalp cache's frames on the way), and keeps every frame.
    std::vector<std::string> simulateNodes() const { return session().simulateNodes(); }
    // Simulate Range: every Simulate node stepped on to `lastFrame` (what the timeline spans);
    // `progress` after each frame (false stops: the frames made stay).
    SimRun simulateTo(double lastFrame, const SimProgress& progress = {});
    // Drops every node's frames and the bakes the groom names: the next frame shown simulates again.
    void invalidateSimulation();
    std::vector<SimStatus> simulationStatus() const { return session().simulationStatus(); }
    // Bake: every Simulate node simulated to `lastFrame`, its frames (from its start frame)
    // written as an Alembic guide cache (sim_cache.h) to simulationCachePath(node); the session
    // reads them from there, and saving names them in the file ("simCaches"), which is what
    // renders and farms read (the Arnold procedural, the CLI): simulate, bake, render. A bake made
    // with other settings than the node has now is ignored (it simulates). Returns the files;
    // throws std::runtime_error (nothing baked when the simulation stopped or failed).
    std::vector<std::string> bakeSimulation(double lastFrame, const SimProgress& progress = {});
    // Where a node's bake goes: "<groom folder>/<stem>_sim/<stem>_<node>.abc" (an unsaved groom:
    // the system's temp folder).
    std::string simulationCachePath(const std::string& node) const;
    // The bakes the groom names (absolute paths), by node.
    const std::map<std::string, SimCacheRef>& simulationBakes() const { return simBakes_; }

private:
    // Unreal's groom or Unity's strands through the core's writer (engine_export.h).
    ExportResult exportEngineGroom(const std::string& path, const ExportOptions& options, bool unity, const ExportProgress& progress);
    // The groom at `seconds` for an export: the time fed to the session, the strands at
    // `density` (full density on the session's own document), the scalp of that time.
    StrandsPtr exportFrame(double seconds, float density, MeshPtr* scalp);
    void adoptBake();  // the bake the file names, else one at the default place: taken as the last bake
    explicit GroomDocument(GroomFile file, std::string path);
    void loadGraph(const std::string& text);
    std::string applyGraph();  // the JSON to the session; "" or the error
    nlohmann::json* nodeJson(const std::string& node);
    const nlohmann::json* nodeJson(const std::string& node) const;

    struct Edit {
        std::string node, name;
        nlohmann::json before, after;
        bool bypass = false;  // an edit of the node's bypass, not a param
        // A stroke or guide edit instead (the texts before and after), named by `label`.
        std::shared_ptr<const StrokeEdit> stroke;
        std::string label;
        // A structural edit instead: the whole graph before and after, named by `label`.
        bool whole = false;
        nlohmann::json graphBefore, graphAfter;
    };
    void record(Edit edit);
    static std::string editName(const Edit& edit);  // "node.param" or the stroke's label
    // The held preview value applied to the graph (nothing when none is held).
    void commitPreview();
    // setParam's work once a preview is out of the way: the JSON, the session, the history.
    std::string applyParam(const std::string& node, const std::string& name, const nlohmann::json& value, bool record);
    struct Preview {
        bool held = false;
        std::string node, name;
        nlohmann::json value;
    };
    Preview preview_;
    // The file's maps into the session: the vertex maps from the scalp's color sets of their
    // names, the texture maps from the catalog (their channels from their files).
    void loadMaps();
    // An edit of the scalp maps as one undo step named `label` (the texts before recorded, as a
    // stroke's are).
    void commitMaps(const std::string& label, const ScalpMaps& maps);
    // A texture stroke's tiles undone or redone: the paint takes them and its file is written.
    void applyTextureEdit(const StrokeEdit& edit, bool after);
    void writeTexturePaint(const std::string& name);
    std::string resolveMapPath(const std::string& stored) const;

    std::unique_ptr<GroomScene> scene_;
    MapCatalog fileCatalog_;  // the catalog as read (guide layers stay in it; a removed map leaves it)
    mutable std::vector<PaintedMap> maps_;
    std::map<std::string, KeptTexturePaint> paints_;  // by map: what the strokes paint on (scalp_paint.h)
    // The texture map sampled per vertex for the scalp display, kept while its source is the same.
    std::uint64_t mapViewKey_ = 0;
    std::shared_ptr<const std::vector<float>> mapViewValues_;
    std::string scalpMapName_, scalpMapChannel_ = "rgb";
    std::string path_;
    nlohmann::json graph_;
    nlohmann::json specs_;  // by type
    mutable std::map<std::string, nlohmann::json> resolvedSpecs_;  // a Compound's or Input's, by its node JSON
    std::vector<Edit> edits_;
    std::size_t undoIndex_ = 0;
    bool dirty_ = false;
    std::optional<std::vector<TransferMarker>> transferMarkers_;
    std::optional<std::string> referencePhotos_;  // set since the scene was made  // set since the scene was made
    DisplayBuilder display_;
    std::vector<GuideId> selection_;
    std::uint64_t selectionVersion_ = 1;
    std::set<std::string> hiddenGroups_;
    double time_ = 0.0;
    std::optional<CardBakeFiles> bake_;
    std::map<std::string, SimCacheRef> simBakes_;  // the Simulate nodes' bakes the file names, by node
};

}  // namespace groomlab::app

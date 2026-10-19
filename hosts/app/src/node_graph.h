// SPDX-License-Identifier: Apache-2.0
#pragma once

// The node graph drawn straight over the viewport: no canvas, no panel, the groom shows between
// the nodes. Nodes are rounded cards with a header in their category's color (playful.json's
// graph section) and their ports down the sides; wires are curves in the port's color; the
// graph's backdrops (a preset's, Maya's Graph Editor's) are tinted frames behind the nodes. The
// view pans and zooms on its own, apart from the camera.
//
// Interaction, while the graph is shown (Tab or the Graph pill; it owns the mouse then, the
// viewport under it does not see clicks; the viewport dims behind it):
//   left drag on empty space       a rubber band: the nodes it touches are selected (with Ctrl or
//                                  Shift: added); a click there deselects (as Maya's Graph Editor)
//   middle drag, wheel             pan, zoom
//   Alt + any button, Alt + wheel  the camera, as everywhere (the events go to the viewport)
//   click a node, drag it          selects it (its params open in a floating panel), moves it (the
//                                  whole selection when it is one of them)
//   Ctrl+click, Shift+click        toggles a node in the selection, adds it
//   drag from an output port       a new wire; dropped on an input, it connects (replacing what
//                                  fed it); a drag from a fed input picks that wire up instead
//                                  (dropped on empty space: disconnected); over a port it cannot
//                                  feed, a line by the cursor says why, and a drop there changes
//                                  nothing (a picked-up wire goes back)
//   click a wire                   selects it; Delete removes it
//   double-click a node            renames it (a glass prompt); a Compound opens instead: its
//                                  nodes back in the graph (Ungroup)
//   right-click                    a node's menu (Rename, Duplicate, Set as Output, View Up to
//                                  Here, Bypass, Frame, Copy, Group, Ungroup, Add Backdrop,
//                                  Delete), a backdrop's (Rename, Color, Delete) or the canvas's
//                                  (Add node, Paste, Add Backdrop, Tidy layout, Frame all)
//   a backdrop's title bar         click selects it, drag moves it with the nodes on it; its
//                                  corner grip resizes it; double-click renames it; Delete
//                                  removes it (as Maya's Graph Editor's backdrops)
//   Delete                         removes the selected nodes (the output node stays)
//   B                              bypasses the selected node (again: back)
//   V                              View Up to Here: the viewport shows the graph's result at the
//                                  selected node (again, or V on the shown one: the output again)
//   Ctrl+C, Ctrl+V                 copies the selected nodes with the links between them (as
//                                  text, Maya's Graph Editor's clipboard), pastes them beside
//   Ctrl+D                         duplicates the selection beside it
//   Ctrl+L                         tidies the layout (every position dropped, laid out afresh)
//   Ctrl+G, Ctrl+Shift+G           groups the selected nodes into a Compound (GraphEdit::collapse,
//                                  Maya's Graph Editor's too), ungroups the selected Compounds
//   Tab                            the add-node search at the cursor (Enter adds, connected after
//                                  the selected node when their ports match; Escape closes),
//                                  ranked as Maya's (nodeSearchRank), each type by its label
//   F                              frames the selected node (none: the whole graph), as Maya's
//   A                              frames the whole graph
//   Escape                         closes the search, else hides the graph
// A node whose evaluation failed wears a red badge (the theme's graph.error) and a red ring; its
// error shows in its tooltip and in the Node panel. Links that run backwards (to a node left of
// their source) loop under the two nodes instead of through them.
// Every edit goes through GroomDocument (one undo step each); a move (of a node or a backdrop)
// and a backdrop's resize are not undo steps.
// Hidden, the graph takes no events at all: the viewport gets them as if it were not there.

#include <functional>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <QColor>
#include <QMargins>
#include <QPainterPath>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QTimer;

namespace groomlab::app {

class GlassPanel;
class GroomDocument;
class Viewport;

// Where a node type ranks for the Add Node search `text` (lower first), as Maya's Graph Editor
// ranks it (graph_editor.py search_rank): 0 its type name or label starts with the text, 1 either
// holds it (or the text without spaces is in the name), 2 every word is in its search keywords
// or its category's title (or its name), 3 in its params' names, 4 in its category and description, 5 anywhere (its
// params' descriptions too); -1 no match. `spec` is the type's operator spec. Empty text: 5.
int nodeSearchRank(const QString& text, const nlohmann::json& spec);
// The types the Add Node search lists for `text`, best first, ties (and empty text: everything) in
// the menu's order (category, then the type's place in it); hidden types (Compound, Input) never.
std::vector<std::string> nodeSearchResults(const GroomDocument& document, const QString& text);

class NodeGraphView : public QWidget {
    Q_OBJECT
public:
    explicit NodeGraphView(QWidget* parent = nullptr);

    void setDocument(GroomDocument* document);  // null: nothing to draw
    void setViewport(Viewport* viewport);       // where Alt-drags go
    // The document's graph changed (an edit, undo, another graph): the cards follow it.
    void refresh();
    // The current node (the one whose params show): the last one clicked of the selection.
    const std::string& selectedNode() const { return selected_; }
    void selectNode(const std::string& name);  // "" none; emits selectionChanged when it changed
    // Every selected node, in the order they were picked (the current one among them).
    const std::vector<std::string>& selectedNodes() const { return selection_; }
    // Several at once (the current one the last); unknown names are dropped.
    void setSelection(const std::vector<std::string>& names);
    // The rubber band over a view rectangle, as a drag on empty space makes it (`add`: Ctrl, Shift).
    void selectInRect(const QRectF& widgetRect, bool add);
    void fitAll();
    // F: the selected node framed (zoomed in no further than 1:1); none selected: fitAll.
    void frameSelected();
    qreal viewScale() const { return scale_; }  // scene to widget: widget = scene * viewScale + viewPan
    QPointF viewPan() const { return pan_; }
    // What fitAll keeps clear around the graph (the chrome over the view); unchanged margins
    // change nothing (the view stays where it was panned).
    void setFitMargins(const QMargins& margins);
    QMargins fitMargins() const { return fitMargins_; }
    void openSearch();                         // at the cursor (off the view: its middle)
    void openSearchAt(const QPointF& where);   // at a point of the view
    void closeSearch();
    bool searchOpen() const;
    void setSearchText(const QString& text);  // as typed into the open search
    // The search's rows as shown ("<label>    <category>") and their types, best first.
    QStringList searchRows() const;
    std::vector<std::string> searchTypes() const;
    QString searchAbout() const;  // the line under the list: the current row's description
    int nodeCount() const { return static_cast<int>(nodes_.size()); }
    int backdropCount() const { return static_cast<int>(backdrops_.size()); }
    int wireCount() const { return static_cast<int>(wires_.size()); }  // the links drawn
    // Each node's time in its last run (ms, by name): a badge in its header (Maya's heat badges).
    void setNodeTimes(const std::vector<std::pair<std::string, double>>& times);
    // Each node's note from the last evaluation (it does nothing as set: a Clump with no centers):
    // a grey badge on its corner.
    void setNodeNotes(const std::vector<std::pair<std::string, std::string>>& notes);
    std::string nodeNote(const std::string& node) const;
    // Each failing node's error from the last evaluation (GroomDocument::nodeErrors): a red badge.
    void setNodeErrors(const std::vector<std::pair<std::string, std::string>>& errors);
    std::string nodeError(const std::string& node) const;
    // View Up to Here: the node the viewport shows ("" the output). Emits viewNodeChanged.
    void setViewedNode(const std::string& node);
    const std::string& viewedNode() const { return viewed_; }
    void toggleViewSelected();  // V
    // Copy and paste (Ctrl+C, Ctrl+V): the clipboard's text, as Maya's Graph Editor writes it.
    void copySelected();
    void paste();
    void deleteSelected();  // Delete: the selected nodes (or the selected wire)
    // While a wire is dragged over a port: why it cannot connect there ("" when it can, or when
    // no wire is over a port). connectionProblem says it for the output (node, port) to the input
    // (node, port) by index: "" when they fit.
    QString wireHint() const { return wireHint_; }
    QString connectionProblem(int fromNode, int fromPort, int toNode, int toPort) const;
    // Pans so the current node is not under `area` (view coordinates: the Node panel), when it is.
    void keepSelectedClearOf(const QRect& area);
    // A node's card in view coordinates (empty for an unknown name).
    QRectF nodeRect(const std::string& node) const;
    // A wire's path as drawn, scene units (tests: a backward link loops under its nodes).
    QPainterPath wirePathOf(int index) const { return index >= 0 && index < wireCount() ? wires_[index].path : QPainterPath(); }
    // The edits a node's context menu offers (Rename..., Duplicate, Set as Output, Bypass,
    // Delete) and the canvas's (Add node, Tidy layout, Fit), as the menu does them.
    void renameSelected();     // the prompt, then the document's rename
    void duplicateSelected();  // Ctrl+D
    void setSelectedAsOutput();
    void tidyLayout();         // every position dropped: laid out afresh and fitted (Ctrl+L)
    // Ctrl+G: the selected nodes into one Compound (selected after); Ctrl+Shift+G: the selected
    // Compounds' nodes back in the graph (selected after). Each one undo step.
    void groupSelected();
    void ungroupSelected();
    // A backdrop around the selected nodes (none: 400x240 around `at`, a view point; NaN: the
    // view's middle), then the prompt for its title. One undo step.
    void addBackdrop(const QPointF& at = QPointF(std::numeric_limits<qreal>::quiet_NaN(), 0.0));
    // The backdrops by index (the graph's list): selected (-1 none), title, color, frame (scene
    // units), its title bar and resize grip in view coordinates (tests drag them).
    int selectedBackdrop() const { return selectedBackdrop_; }
    void selectBackdrop(int index);
    QString backdropTitle(int index) const;
    QColor backdropColor(int index) const;
    QRectF backdropRect(int index) const;
    QRectF backdropTitleBar(int index) const;
    QRectF backdropGrip(int index) const;
    void renameBackdrop(int index);  // the prompt, then the document's edit
    void setBackdropColor(int index, const QColor& color);
    void removeBackdrop(int index);
    // A glass prompt for a name (Save as Preset): Enter gives it through `done`, Escape drops it.
    void promptName(const QString& title, const QString& placeholder, std::function<void(const QString&)> done);
    bool promptOpen() const;
    void acceptPrompt();  // as Enter does (tests)
    void setPromptText(const QString& text);
    // ---- Compound nodes (node_library.h). Save as Node: a glass form over the view (no dialog)
    // for Compound `node`: label, category, description, author, id, version, license.
    void openSaveNode(const std::string& node);
    bool saveNodeOpen() const;
    void setSaveNodeField(const QString& field, const QString& value);  // as typed into it
    QString saveNodeField(const QString& field) const;
    QString acceptSaveNode();  // as Save does: "" or the error (shown on the form)
    // A library node written over its file as it is now, one version up; the library's newer copy.
    void saveToLibrary(const std::string& node);
    void updateLibraryNode(const std::string& node);
    // .glnode files copied into the user's node folder, each added (at `at`, view coordinates; NaN:
    // the view's middle): Import Node... and a file dropped on the view. The nodes added.
    std::vector<std::string> importNodeFiles(const QStringList& paths,
                                             const QPointF& at = QPointF(std::numeric_limits<qreal>::quiet_NaN(), 0.0));
    // ---- Script nodes (script.h). New Script Node: one from the template (at `at`, view
    // coordinates; NaN: the view's middle), its editor open; the node made.
    std::string newScriptNode(const QPointF& at = QPointF(std::numeric_limits<qreal>::quiet_NaN(), 0.0));
    // Edit Node...: a glass panel over the view (no dialog) with the node's label, what it writes
    // and its script (monospace); live: a change is checked after a pause (300 ms), its errors
    // listed by line and column (the line tinted), and the node takes it when it checks.
    void openScriptEditor(const std::string& node);
    bool scriptEditorOpen() const;
    void setScriptEditorField(const QString& field, const QString& value);  // "label", "writes", "script": as typed
    QString scriptEditorField(const QString& field) const;
    QString scriptEditorErrors() const;  // as listed ("" none)
    QString applyScriptEditor();         // as the pause does: "" or the errors
    QString saveScriptEditor(bool asNew);  // Save to Library / Save as New: "" or the error
    void closeScriptEditor();

signals:
    void selectionChanged(const std::string& node);
    // The graph changed through this view: the host evaluates and refreshes what shows it.
    void edited();
    void message(const QString& text);
    void hideRequested();
    // View Up to Here changed: the host evaluates the graph up to that node ("" the output).
    void viewNodeChanged(const std::string& node);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;  // a node's header: rename
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool event(QEvent* event) override;  // a node's tooltip: its error or note
    bool eventFilter(QObject* watched, QEvent* event) override;  // the search's keys
    void dragEnterEvent(QDragEnterEvent* event) override;  // .glnode files: imported and added
    void dropEvent(QDropEvent* event) override;

private:
    struct Port {
        std::string name, type;
        bool optional = false;
    };
    struct Node {
        std::string name, type, category;
        bool bypass = false;
        std::string evaluation = "always";  // when it runs: always, viewportOnly, renderOnly
        QPointF pos;   // scene units (the card's top-left)
        QSizeF size;
        std::vector<Port> inputs, outputs;
        std::string libraryLabel;      // a node from the node library: its label
        bool updateAvailable = false;  // the library has a newer copy of it
    };
    struct Wire {
        int from = -1, to = -1;  // node indices
        int output = 0, input = 0;  // port indices
        QPainterPath path;         // scene units
    };
    // A backdrop framing part of the graph (the graph's "backdrops", graph_io.h: Maya's Graph
    // Editor's and a preset's): drawn behind the nodes, layout only.
    struct Backdrop {
        QString title;
        QRectF rect;  // scene units
        QColor color;
    };
    enum class Drag { None, Pan, Move, Wire, Camera, Band, Backdrop, BackdropResize };

    void rebuild();      // nodes_, wires_ and backdrops_ from the document
    void autoLayout();   // positions for nodes the document gives none
    QPointF toScene(const QPointF& widget) const;
    QPointF toWidget(const QPointF& scene) const;
    QPointF inputPos(const Node& node, int port) const;   // scene units, the port's center
    QPointF outputPos(const Node& node, int port) const;
    // A wire from an output to an input; one running backwards loops under `below` (scene y:
    // the lowest edge of the nodes it joins; NaN: a little under its ends).
    QPainterPath wirePath(const QPointF& from, const QPointF& to, qreal below = std::numeric_limits<qreal>::quiet_NaN()) const;
    QPainterPath wirePathFor(const Wire& wire) const;
    int nodeIndex(const std::string& name) const;
    bool isSelected(const std::string& name) const;
    void updateWireHint(const QPointF& scene);
    int nodeAt(const QPointF& scene) const;
    // A port under a scene point: the node and port indices, output or input.
    bool portAt(const QPointF& scene, int* node, int* port, bool* output) const;
    int wireAt(const QPointF& scene) const;
    bool bypassButtonAt(const QPointF& scene, int* node) const;
    // The backdrop whose title bar (or, `grip`, resize corner) is under a scene point; -1 none.
    int backdropTitleAt(const QPointF& scene, bool* grip = nullptr) const;
    int backdropAt(const QPointF& scene) const;  // anywhere on it
    QRectF bypassButtonRect(const Node& node) const;
    void drawNode(QPainter& painter, const Node& node, bool selected);
    void forwardToViewport(QEvent* event);
    void addFromSearch();
    void filterSearch(const QString& text);
    void setBypass(const std::string& node, bool bypass);
    void setEvaluation(const std::string& node, const std::string& mode);

    GroomDocument* document_ = nullptr;
    Viewport* viewport_ = nullptr;
    GlassPanel* saveNode_ = nullptr;          // the Save as Node form (made as it opens)
    std::map<QString, QWidget*> saveFields_;  // its fields by name
    QLabel* saveError_ = nullptr;
    std::string saveNodeName_;                // the compound it saves
    bool saveIdTyped_ = false;                // the id typed (no longer follows label and author)
    GlassPanel* scriptEditor_ = nullptr;      // the script node editor (made as it opens)
    std::map<QString, QWidget*> scriptFields_;
    QLabel* scriptErrors_ = nullptr;
    QLabel* scriptStatus_ = nullptr;
    QTimer* scriptPause_ = nullptr;
    std::string scriptNode_;                  // the Script node it edits
    std::vector<Node> nodes_;
    std::vector<Wire> wires_;
    std::vector<Backdrop> backdrops_;
    std::map<std::string, double> times_;     // ms per node in its last run
    std::map<std::string, std::string> notes_;  // a note per node from its last run
    std::map<std::string, std::string> errors_;  // an error per failing node
    std::string viewed_;  // View Up to Here ("" the output)
    std::map<std::string, QPointF> laidOut_;  // positions the view chose (the document has none)
    std::string selected_;
    std::vector<std::string> selection_;
    QRectF band_;  // the rubber band (view coordinates) while it drags
    bool bandAdds_ = false;
    QPointF moveStart_;  // the pressed node's position when a move began
    QString wireHint_;
    int selectedWire_ = -1;
    int selectedBackdrop_ = -1;
    // A backdrop being moved (the nodes on it carried along, by index) or resized.
    int dragBackdrop_ = -1;
    QRectF backdropStart_;
    std::vector<int> carried_;
    qreal scale_ = 1.0;
    QPointF pan_;
    QMargins fitMargins_{60, 90, 60, 100};
    bool fitted_ = false;
    Drag drag_ = Drag::None;
    QPointF last_;        // widget units
    QPointF pressScene_;  // where the press was
    int dragNode_ = -1;
    // A wire being dragged: from an output port (dragWireFrom_, dragWirePort_) to the cursor;
    // picked up from an input (dragWireInputNode_ >= 0), that connection goes when dropped away.
    int dragWireFrom_ = -1, dragWirePort_ = -1;
    int dragWireInputNode_ = -1, dragWireInputPort_ = -1;
    QPointF dragWireTo_;  // scene units
    // The add-node search.
    GlassPanel* search_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QListWidget* searchList_ = nullptr;
    QLabel* searchAbout_ = nullptr;  // the current type's description under the list
    QPointF searchScene_;
    // The name prompt.
    GlassPanel* prompt_ = nullptr;
    QLabel* promptTitle_ = nullptr;
    QLineEdit* promptEdit_ = nullptr;
    std::function<void(const QString&)> promptDone_;
};

}  // namespace groomlab::app

// SPDX-License-Identifier: Apache-2.0
#pragma once

// The node graph drawn straight over the viewport: no canvas, no panel, the groom shows between
// the nodes. Nodes are rounded cards with a header in their category's color (playful.json's
// graph section) and their ports down the sides; wires are curves in the port's color; the
// graph's backdrops (a preset's, Maya's Graph Editor's) are tinted frames behind the nodes. The
// view pans and zooms on its own, apart from the camera.
//
// Interaction, while the graph is shown (Tab or the Graph pill; it owns the mouse then, the
// viewport under it does not see clicks):
//   left drag on empty space       pans (a click there deselects)
//   middle drag, wheel             pan, zoom
//   Alt + any button, Alt + wheel  the camera, as everywhere (the events go to the viewport)
//   click a node, drag it          selects it (its params open in a floating panel), moves it
//   drag from an output port       a new wire; dropped on an input, it connects (replacing what
//                                  fed it); a drag from a fed input picks that wire up instead
//                                  (dropped on empty space: disconnected)
//   click a wire                   selects it; Delete removes it
//   double-click a node            renames it (a glass prompt)
//   right-click                    a node's menu (Rename, Duplicate, Set as Output, Bypass, Delete)
//                                  or the canvas's (Add node, Tidy layout, Fit)
//   Delete                         removes the selected node (the output node stays)
//   B                              bypasses the selected node (again: back)
//   Ctrl+D                         duplicates the selected node beside it
//   Ctrl+L                         tidies the layout (every position dropped, laid out afresh)
//   Tab                            the add-node search at the cursor (Enter adds, connected after
//                                  the selected node when their ports match; Escape closes)
//   F                              fits the whole graph in the view
//   Escape                         closes the search, else hides the graph
// Every edit goes through GroomDocument (one undo step each); a move is not an undo step.
// Hidden, the graph takes no events at all: the viewport gets them as if it were not there.

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <QColor>
#include <QMargins>
#include <QPainterPath>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;

namespace groomlab::app {

class GlassPanel;
class GroomDocument;
class Viewport;

class NodeGraphView : public QWidget {
    Q_OBJECT
public:
    explicit NodeGraphView(QWidget* parent = nullptr);

    void setDocument(GroomDocument* document);  // null: nothing to draw
    void setViewport(Viewport* viewport);       // where Alt-drags go
    // The document's graph changed (an edit, undo, another graph): the cards follow it.
    void refresh();
    const std::string& selectedNode() const { return selected_; }
    void selectNode(const std::string& name);  // "" none; emits selectionChanged when it changed
    void fitAll();
    // What fitAll keeps clear around the graph (the chrome over the view).
    void setFitMargins(const QMargins& margins);
    void openSearch();
    void closeSearch();
    bool searchOpen() const;
    int nodeCount() const { return static_cast<int>(nodes_.size()); }
    int backdropCount() const { return static_cast<int>(backdrops_.size()); }
    int wireCount() const { return static_cast<int>(wires_.size()); }  // the links drawn
    // Each node's time in its last run (ms, by name): a badge in its header (Maya's heat badges).
    void setNodeTimes(const std::vector<std::pair<std::string, double>>& times);
    // Each node's note from the last evaluation (it does nothing as set: a Clump with no centers):
    // a grey badge on its corner.
    void setNodeNotes(const std::vector<std::pair<std::string, std::string>>& notes);
    std::string nodeNote(const std::string& node) const;
    // The edits a node's context menu offers (Rename..., Duplicate, Set as Output, Bypass,
    // Delete) and the canvas's (Add node, Tidy layout, Fit), as the menu does them.
    void renameSelected();     // the prompt, then the document's rename
    void duplicateSelected();  // Ctrl+D
    void setSelectedAsOutput();
    void tidyLayout();         // every position dropped: laid out afresh and fitted (Ctrl+L)
    // A glass prompt for a name (Save as Preset): Enter gives it through `done`, Escape drops it.
    void promptName(const QString& title, const QString& placeholder, std::function<void(const QString&)> done);
    bool promptOpen() const;
    void acceptPrompt();  // as Enter does (tests)
    void setPromptText(const QString& text);

signals:
    void selectionChanged(const std::string& node);
    // The graph changed through this view: the host evaluates and refreshes what shows it.
    void edited();
    void message(const QString& text);
    void hideRequested();

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
    bool eventFilter(QObject* watched, QEvent* event) override;  // the search's keys

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
    enum class Drag { None, Pan, Move, Wire, Camera };

    void rebuild();      // nodes_, wires_ and backdrops_ from the document
    void autoLayout();   // positions for nodes the document gives none
    QPointF toScene(const QPointF& widget) const;
    QPointF toWidget(const QPointF& scene) const;
    QPointF inputPos(const Node& node, int port) const;   // scene units, the port's center
    QPointF outputPos(const Node& node, int port) const;
    QPainterPath wirePath(const QPointF& from, const QPointF& to) const;
    int nodeAt(const QPointF& scene) const;
    // A port under a scene point: the node and port indices, output or input.
    bool portAt(const QPointF& scene, int* node, int* port, bool* output) const;
    int wireAt(const QPointF& scene) const;
    bool bypassButtonAt(const QPointF& scene, int* node) const;
    QRectF bypassButtonRect(const Node& node) const;
    void drawNode(QPainter& painter, const Node& node, bool selected);
    void forwardToViewport(QEvent* event);
    void addFromSearch();
    void filterSearch(const QString& text);
    void setBypass(const std::string& node, bool bypass);
    void setEvaluation(const std::string& node, const std::string& mode);

    GroomDocument* document_ = nullptr;
    Viewport* viewport_ = nullptr;
    std::vector<Node> nodes_;
    std::vector<Wire> wires_;
    std::vector<Backdrop> backdrops_;
    std::map<std::string, double> times_;     // ms per node in its last run
    std::map<std::string, std::string> notes_;  // a note per node from its last run
    std::map<std::string, QPointF> laidOut_;  // positions the view chose (the document has none)
    std::string selected_;
    int selectedWire_ = -1;
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
    QPointF searchScene_;
    // The name prompt.
    GlassPanel* prompt_ = nullptr;
    QLabel* promptTitle_ = nullptr;
    QLineEdit* promptEdit_ = nullptr;
    std::function<void(const QString&)> promptDone_;
};

}  // namespace groomlab::app

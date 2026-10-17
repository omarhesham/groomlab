// SPDX-License-Identifier: Apache-2.0
#include "node_graph.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <tuple>

#include <QClipboard>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QToolTip>
#include <QMenu>
#include <QLabel>
#include <QLineF>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "glass.h"
#include "groom_document.h"
#include "theme.h"
#include "viewport.h"

#include "groomlab/graph.h"
#include "groomlab/node_catalog.h"

namespace groomlab::app {

namespace {

constexpr qreal kNodeWidth = 160.0;
constexpr qreal kHeader = 28.0;
constexpr qreal kRow = 18.0;
constexpr qreal kBodyPadding = 8.0;
constexpr qreal kPortRadius = 5.0;
constexpr qreal kColumn = 230.0;  // the auto layout's columns and rows
constexpr qreal kRowGap = 120.0;

// Dark words on a bright header (yellow, white, emerald), white on the others; a tinted header
// (a wash of the category color) takes the theme's text.
QColor headerText(const QColor& fill) { return theme::tintedHeaders() ? theme::text() : theme::readableOn(fill); }

// Where pasted and duplicated nodes go: beside what they were copied from (Maya's paste offset).
constexpr qreal kPasteOffsetX = 40.0, kPasteOffsetY = 60.0;

// A backdrop's title bar and its resize grip (the corner), scene units, as Maya's BackdropItem.
constexpr qreal kBackdropHeader = 28.0, kBackdropGrip = 14.0;
// Its colors as Maya's backdrop menu offers them.
const std::pair<const char*, QColor> kBackdropColors[] = {
    {QT_TRANSLATE_NOOP("NodeGraphView", "Blue"), QColor(70, 90, 120)},     {QT_TRANSLATE_NOOP("NodeGraphView", "Green"), QColor(70, 120, 80)},
    {QT_TRANSLATE_NOOP("NodeGraphView", "Purple"), QColor(110, 80, 130)}, {QT_TRANSLATE_NOOP("NodeGraphView", "Amber"), QColor(140, 110, 50)},
    {QT_TRANSLATE_NOOP("NodeGraphView", "Gray"), QColor(90, 92, 98)}};

}  // namespace

NodeGraphView::NodeGraphView(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_NoSystemBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);

    // The add-node search: a glass box with a filter line and the matching types.
    search_ = new GlassPanel(this, 14.0);
    search_->setColor(theme::glassSolid());
    auto* layout = new QVBoxLayout(search_);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    searchEdit_ = new QLineEdit(search_);
    searchEdit_->setPlaceholderText(tr("Add a node..."));
    searchEdit_->setFont(theme::uiFont(14, 600));
    searchList_ = new QListWidget(search_);
    searchList_->setFont(theme::uiFont(13, 500));
    // The current type's description under the list (Maya's search shows it there too).
    searchAbout_ = new QLabel(search_);
    searchAbout_->setWordWrap(true);
    searchAbout_->setFont(theme::uiFont(11, 500));
    searchAbout_->setStyleSheet(QStringLiteral("color: %1;").arg(theme::textMuted().name()));
    searchAbout_->setMinimumHeight(44);
    searchAbout_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(searchEdit_);
    layout->addWidget(searchList_, 1);
    layout->addWidget(searchAbout_);
    search_->resize(320, 380);
    search_->hide();
    connect(searchList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item, QListWidgetItem*) {
        if (item) searchAbout_->setText(item->toolTip());
    });
    connect(searchEdit_, &QLineEdit::textChanged, this, &NodeGraphView::filterSearch);
    connect(searchEdit_, &QLineEdit::returnPressed, this, &NodeGraphView::addFromSearch);
    connect(searchList_, &QListWidget::itemActivated, this, [this](QListWidgetItem*) { addFromSearch(); });
    searchEdit_->installEventFilter(this);
    searchList_->installEventFilter(this);

    // The name prompt (Save as Preset): a title, a line, Enter or Escape; no dialog.
    prompt_ = new GlassPanel(this, 14.0);
    prompt_->setColor(theme::glassSolid());
    auto* promptLayout = new QVBoxLayout(prompt_);
    promptLayout->setContentsMargins(14, 12, 14, 12);
    promptLayout->setSpacing(8);
    promptTitle_ = new QLabel(prompt_);
    promptTitle_->setFont(theme::uiFont(14, 800));
    promptTitle_->setStyleSheet(QStringLiteral("color: %1;").arg(theme::text().name()));
    promptEdit_ = new QLineEdit(prompt_);
    promptEdit_->setFont(theme::uiFont(14, 600));
    auto* hint = new QLabel(tr("Enter saves, Esc cancels"), prompt_);
    hint->setFont(theme::uiFont(11, 600));
    hint->setStyleSheet(QStringLiteral("color: %1;").arg(theme::textMuted().name()));
    promptLayout->addWidget(promptTitle_);
    promptLayout->addWidget(promptEdit_);
    promptLayout->addWidget(hint);
    prompt_->resize(300, 104);
    prompt_->hide();
    connect(promptEdit_, &QLineEdit::returnPressed, this, &NodeGraphView::acceptPrompt);
    promptEdit_->installEventFilter(this);
}

// ---- The name prompt ----------------------------------------------------------------------------

void NodeGraphView::promptName(const QString& title, const QString& placeholder, std::function<void(const QString&)> done) {
    promptDone_ = std::move(done);
    promptTitle_->setText(title);
    promptEdit_->setPlaceholderText(placeholder);
    promptEdit_->clear();
    prompt_->move((width() - prompt_->width()) / 2, std::max(0, height() / 2 - prompt_->height()));
    prompt_->show();
    prompt_->raise();
    promptEdit_->setFocus();
}

bool NodeGraphView::promptOpen() const { return !prompt_->isHidden(); }

void NodeGraphView::acceptPrompt() {
    if (!promptOpen()) return;
    const QString text = promptEdit_->text().trimmed();
    prompt_->hide();
    setFocus();
    std::function<void(const QString&)> done;
    done.swap(promptDone_);
    if (done && !text.isEmpty()) done(text);
}

void NodeGraphView::setPromptText(const QString& text) { promptEdit_->setText(text); }

void NodeGraphView::setDocument(GroomDocument* document) {
    document_ = document;
    laidOut_.clear();
    selected_.clear();
    selection_.clear();
    selectedWire_ = -1;
    fitted_ = false;
    refresh();
}

void NodeGraphView::setViewport(Viewport* viewport) { viewport_ = viewport; }

void NodeGraphView::refresh() {
    rebuild();
    if (!fitted_ && !nodes_.empty() && width() > 0) fitAll();
    update();
}

void NodeGraphView::rebuild() {
    nodes_.clear();
    wires_.clear();
    backdrops_.clear();
    if (!document_) return;
    // The backdrops as the graph keeps them (title, a rectangle, an optional color).
    const nlohmann::json& graph = document_->graph();
    if (graph.contains("backdrops") && graph["backdrops"].is_array()) {
        for (const nlohmann::json& b : graph["backdrops"]) {
            if (!b.is_object()) continue;
            auto number = [&](const char* key, double fallback) { return b.contains(key) && b[key].is_number() ? b[key].get<double>() : fallback; };
            Backdrop drop;
            drop.title = QString::fromStdString(b.value("title", std::string("Backdrop")));
            drop.rect = QRectF(number("x", 0.0), number("y", 0.0), std::max(1.0, number("w", 200.0)), std::max(1.0, number("h", 150.0)));
            drop.color = theme::nodeBody().lighter(140);
            if (b.contains("color") && b["color"].is_array() && b["color"].size() == 3 && b["color"][0].is_number()) {
                drop.color = QColor(std::clamp(b["color"][0].get<int>(), 0, 255), std::clamp(b["color"][1].get<int>(), 0, 255),
                                    std::clamp(b["color"][2].get<int>(), 0, 255));
            }
            backdrops_.push_back(std::move(drop));
        }
    }
    if (selectedBackdrop_ >= static_cast<int>(backdrops_.size())) selectedBackdrop_ = -1;
    for (const GroomDocument::NodeInfo& info : document_->nodes()) {
        Node node;
        node.name = info.name;
        node.type = info.type;
        node.bypass = info.bypass;
        node.evaluation = info.evaluation;
        // The node's ports as it is set (its exposed params' after its own: Field, Curve or Value).
        if (const nlohmann::json* spec = document_->nodeSpec(info.name)) {
            node.category = spec->value("category", std::string());
            for (const nlohmann::json& p : spec->value("inputs", nlohmann::json::array())) {
                node.inputs.push_back({p.value("name", std::string()), p.value("type", std::string()), p.value("optional", false)});
            }
            for (const nlohmann::json& p : spec->value("outputs", nlohmann::json::array())) {
                node.outputs.push_back({p.value("name", std::string()), p.value("type", std::string()), false});
            }
        }
        if (info.type == "Output") node.category = "output";
        const std::size_t rows = std::max<std::size_t>(std::max(node.inputs.size(), node.outputs.size()), 1);
        node.size = QSizeF(kNodeWidth, kHeader + kBodyPadding * 2 + rows * kRow);
        float x = 0.0f, y = 0.0f;
        if (document_->nodePosition(info.name, &x, &y)) {
            node.pos = QPointF(x, y);
        } else if (const auto it = laidOut_.find(info.name); it != laidOut_.end()) {
            node.pos = it->second;
        } else {
            node.pos = QPointF(std::nan(""), 0.0);  // laid out below
        }
        nodes_.push_back(std::move(node));
    }
    autoLayout();
    auto index = [this](const std::string& name) {
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].name == name) return static_cast<int>(i);
        }
        return -1;
    };
    for (const GroomDocument::ConnectionInfo& c : document_->connections()) {
        Wire wire;
        wire.from = index(c.from);
        wire.to = index(c.to);
        if (wire.from < 0 || wire.to < 0) continue;
        const Node& from = nodes_[wire.from];
        const Node& to = nodes_[wire.to];
        wire.output = 0;
        for (std::size_t i = 0; i < from.outputs.size(); ++i) {
            if (from.outputs[i].name == c.output) wire.output = static_cast<int>(i);
        }
        wire.input = -1;
        for (std::size_t i = 0; i < to.inputs.size(); ++i) {
            if (to.inputs[i].name == c.input) wire.input = static_cast<int>(i);
        }
        if (wire.input < 0) continue;  // a Field port the spec does not list: not drawn
        wire.path = wirePathFor(wire);
        wires_.push_back(wire);
    }
    if (selectedWire_ >= static_cast<int>(wires_.size())) selectedWire_ = -1;
    selection_.erase(std::remove_if(selection_.begin(), selection_.end(), [&](const std::string& n) { return index(n) < 0; }), selection_.end());
    if (!selected_.empty() && index(selected_) < 0) {
        selected_ = selection_.empty() ? std::string() : selection_.back();
        emit selectionChanged(selected_);
    }
    // A viewed node gone (removed, renamed): the output shows again.
    if (!viewed_.empty() && index(viewed_) < 0) setViewedNode({});
}

void NodeGraphView::autoLayout() {
    // Nodes without a position: columns by how far they are from the sources (the longest path
    // in), rows in the graph's order; placed to the right of what has a position already.
    std::vector<int> depth(nodes_.size(), 0);
    if (document_) {
        std::map<std::string, int> index;
        for (std::size_t i = 0; i < nodes_.size(); ++i) index[nodes_[i].name] = static_cast<int>(i);
        const std::vector<GroomDocument::ConnectionInfo> connections = document_->connections();
        // Relax the longest paths: graphs are acyclic and small.
        for (std::size_t pass = 0; pass < nodes_.size() + 1; ++pass) {
            bool changed = false;
            for (const GroomDocument::ConnectionInfo& c : connections) {
                const auto a = index.find(c.from), b = index.find(c.to);
                if (a == index.end() || b == index.end()) continue;
                if (depth[b->second] < depth[a->second] + 1) {
                    depth[b->second] = depth[a->second] + 1;
                    changed = true;
                }
            }
            if (!changed) break;
        }
    }
    // A long chain wraps into blocks of columns, one under the other, so the whole graph fits
    // the view at a readable scale (a chain of fourteen in one row would not).
    constexpr int kColumnsPerBlock = 7;
    std::map<int, int> rows;
    std::map<int, int> blockRows;  // the tallest column in each block, for the block's height
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (std::isnan(nodes_[i].pos.x())) blockRows[depth[i] / kColumnsPerBlock] = std::max(blockRows[depth[i] / kColumnsPerBlock], ++rows[depth[i]]);
    }
    rows.clear();
    std::map<int, qreal> blockTop;
    qreal top = 0.0;
    for (const auto& [block, count] : blockRows) {
        blockTop[block] = top;
        top += count * kRowGap + 60.0;
    }
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        Node& node = nodes_[i];
        if (!std::isnan(node.pos.x())) continue;
        const int row = rows[depth[i]]++;
        node.pos = QPointF((depth[i] % kColumnsPerBlock) * kColumn, blockTop[depth[i] / kColumnsPerBlock] + row * kRowGap);
        laidOut_[node.name] = node.pos;
    }
}

QPointF NodeGraphView::toScene(const QPointF& widget) const { return (widget - pan_) / scale_; }

QPointF NodeGraphView::toWidget(const QPointF& scene) const { return scene * scale_ + pan_; }

QPointF NodeGraphView::inputPos(const Node& node, int port) const {
    return QPointF(node.pos.x(), node.pos.y() + kHeader + kBodyPadding + kRow * (port + 0.5));
}

QPointF NodeGraphView::outputPos(const Node& node, int port) const {
    return QPointF(node.pos.x() + node.size.width(), node.pos.y() + kHeader + kBodyPadding + kRow * (port + 0.5));
}

QPainterPath NodeGraphView::wirePath(const QPointF& from, const QPointF& to, qreal below) const {
    QPainterPath path(from);
    if (to.x() >= from.x() + 30.0) {
        const qreal dx = std::max(60.0, std::abs(to.x() - from.x()) * 0.5);
        path.cubicTo(from + QPointF(dx, 0), to - QPointF(dx, 0), to);
        return path;
    }
    // Backwards (the input left of the output): out to the right, down under both nodes, back
    // along under them, up into the input from its left, so the wire never crosses the cards.
    const qreal y = std::isnan(below) ? std::max(from.y(), to.y()) + 60.0 : below + 24.0;
    constexpr qreal r = 40.0;
    path.cubicTo(from + QPointF(r, 0), QPointF(from.x() + r, y), QPointF(from.x(), y));
    path.lineTo(to.x(), y);
    path.cubicTo(QPointF(to.x() - r, y), to - QPointF(r, 0), to);
    return path;
}

QPainterPath NodeGraphView::wirePathFor(const Wire& wire) const {
    const Node& from = nodes_[wire.from];
    const Node& to = nodes_[wire.to];
    const qreal below = std::max(from.pos.y() + from.size.height(), to.pos.y() + to.size.height());
    return wirePath(outputPos(from, wire.output), inputPos(to, wire.input), below);
}

int NodeGraphView::nodeIndex(const std::string& name) const {
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

bool NodeGraphView::isSelected(const std::string& name) const {
    return std::find(selection_.begin(), selection_.end(), name) != selection_.end();
}

QRectF NodeGraphView::nodeRect(const std::string& node) const {
    const int i = nodeIndex(node);
    if (i < 0) return {};
    return QRectF(toWidget(nodes_[i].pos), nodes_[i].size * scale_);
}

void NodeGraphView::keepSelectedClearOf(const QRect& area) {
    // The current node under the Node panel: the graph pans left (and up or down no further than
    // the view) until the node is beside the panel, as Maya keeps the selection in sight.
    const QRectF node = nodeRect(selected_);
    if (node.isEmpty() || !node.intersects(QRectF(area))) return;
    const qreal shift = node.right() - area.left() + 24.0;
    if (node.left() - shift < 8.0 && node.width() + 32.0 > area.left()) return;  // no room left of the panel
    pan_.rx() -= shift;
    update();
}

void NodeGraphView::fitAll() {
    if (nodes_.empty() || width() <= 0 || height() <= 0) return;
    QRectF bounds;
    for (const Node& n : nodes_) bounds |= QRectF(n.pos, n.size);
    for (const Backdrop& b : backdrops_) bounds |= b.rect;
    bounds.adjust(-40, -40, 40, 40);
    // Room for the chrome around: the top row, the bottom pill, the look card.
    const QRectF view(fitMargins_.left(), fitMargins_.top(), std::max(100, width() - fitMargins_.left() - fitMargins_.right()),
                      std::max(100, height() - fitMargins_.top() - fitMargins_.bottom()));
    // Never so small that the names cannot be read: a bigger graph starts at its left.
    scale_ = std::clamp(std::min(view.width() / bounds.width(), view.height() / bounds.height()), 0.55, 1.0);
    pan_ = view.center() - bounds.center() * scale_;
    if (bounds.width() * scale_ > view.width()) pan_.setX(view.left() - bounds.left() * scale_);
    fitted_ = true;
    update();
}

void NodeGraphView::frameSelected() {
    int index = -1;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].name == selected_) index = static_cast<int>(i);
    }
    if (index < 0 || width() <= 0 || height() <= 0) {
        fitAll();
        return;
    }
    const QRectF bounds = QRectF(nodes_[index].pos, nodes_[index].size).adjusted(-60, -60, 60, 60);
    const QRectF view(fitMargins_.left(), fitMargins_.top(), std::max(100, width() - fitMargins_.left() - fitMargins_.right()),
                      std::max(100, height() - fitMargins_.top() - fitMargins_.bottom()));
    scale_ = std::clamp(std::min(view.width() / bounds.width(), view.height() / bounds.height()), 0.55, 1.0);
    pan_ = view.center() - bounds.center() * scale_;
    fitted_ = true;
    update();
}

void NodeGraphView::setFitMargins(const QMargins& margins) {
    if (margins == fitMargins_) return;
    fitMargins_ = margins;
    if (fitted_ && isVisible()) fitAll();
}

void NodeGraphView::selectNode(const std::string& name) {
    const std::vector<std::string> only = name.empty() ? std::vector<std::string>() : std::vector<std::string>{name};
    if (selectedBackdrop_ >= 0) {
        selectedBackdrop_ = -1;
        update();
    }
    if (name == selected_ && selection_ == only) return;
    selection_ = only;
    selectedWire_ = -1;
    update();
    if (name == selected_) return;
    selected_ = name;
    emit selectionChanged(selected_);
}

void NodeGraphView::setSelection(const std::vector<std::string>& names) {
    std::vector<std::string> kept;
    for (const std::string& n : names) {
        if (nodeIndex(n) >= 0 && std::find(kept.begin(), kept.end(), n) == kept.end()) kept.push_back(n);
    }
    selection_ = kept;
    selectedWire_ = -1;
    selectedBackdrop_ = -1;
    update();
    const std::string current = kept.empty() ? std::string() : kept.back();
    if (current == selected_) return;
    selected_ = current;
    emit selectionChanged(selected_);
}

void NodeGraphView::selectInRect(const QRectF& widgetRect, bool add) {
    const QRectF scene(toScene(widgetRect.topLeft()), toScene(widgetRect.bottomRight()));
    std::vector<std::string> names = add ? selection_ : std::vector<std::string>();
    for (const Node& n : nodes_) {
        if (QRectF(n.pos, n.size).intersects(scene.normalized()) && std::find(names.begin(), names.end(), n.name) == names.end()) {
            names.push_back(n.name);
        }
    }
    // The current node stays current when it is still selected (its params keep showing).
    if (!selected_.empty()) {
        const auto it = std::find(names.begin(), names.end(), selected_);
        if (it != names.end()) {
            names.erase(it);
            names.push_back(selected_);
        }
    }
    setSelection(names);
}

int NodeGraphView::nodeAt(const QPointF& scene) const {
    for (int i = static_cast<int>(nodes_.size()) - 1; i >= 0; --i) {
        if (QRectF(nodes_[i].pos, nodes_[i].size).contains(scene)) return i;
    }
    return -1;
}

bool NodeGraphView::portAt(const QPointF& scene, int* node, int* port, bool* output) const {
    const qreal reach = kPortRadius * 2.2;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const Node& n = nodes_[i];
        for (std::size_t p = 0; p < n.outputs.size(); ++p) {
            if (QLineF(outputPos(n, static_cast<int>(p)), scene).length() <= reach) {
                *node = static_cast<int>(i);
                *port = static_cast<int>(p);
                *output = true;
                return true;
            }
        }
        for (std::size_t p = 0; p < n.inputs.size(); ++p) {
            if (QLineF(inputPos(n, static_cast<int>(p)), scene).length() <= reach) {
                *node = static_cast<int>(i);
                *port = static_cast<int>(p);
                *output = false;
                return true;
            }
        }
    }
    return false;
}

int NodeGraphView::wireAt(const QPointF& scene) const {
    QPainterPathStroker stroker;
    stroker.setWidth(10.0);
    for (std::size_t i = 0; i < wires_.size(); ++i) {
        if (stroker.createStroke(wires_[i].path).contains(scene)) return static_cast<int>(i);
    }
    return -1;
}

QRectF NodeGraphView::bypassButtonRect(const Node& node) const {
    return QRectF(node.pos.x() + node.size.width() - 26, node.pos.y() + 5, 18, 18);
}

int NodeGraphView::backdropTitleAt(const QPointF& scene, bool* grip) const {
    for (int i = static_cast<int>(backdrops_.size()) - 1; i >= 0; --i) {
        const QRectF& r = backdrops_[i].rect;
        if (QRectF(r.right() - kBackdropGrip, r.bottom() - kBackdropGrip, kBackdropGrip, kBackdropGrip).contains(scene)) {
            if (grip) *grip = true;
            return i;
        }
        if (QRectF(r.topLeft(), QSizeF(r.width(), kBackdropHeader)).contains(scene)) {
            if (grip) *grip = false;
            return i;
        }
    }
    return -1;
}

int NodeGraphView::backdropAt(const QPointF& scene) const {
    for (int i = static_cast<int>(backdrops_.size()) - 1; i >= 0; --i) {
        if (backdrops_[i].rect.contains(scene)) return i;
    }
    return -1;
}

bool NodeGraphView::bypassButtonAt(const QPointF& scene, int* node) const {
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (bypassButtonRect(nodes_[i]).contains(scene)) {
            *node = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

// ---- Painting -------------------------------------------------------------------------------------

void NodeGraphView::drawNode(QPainter& painter, const Node& node, bool selected) {
    const QRectF rect(node.pos, node.size);
    const bool failed = errors_.count(node.name) > 0;
    const QColor color = theme::nodeCategoryColor(node.category);
    const qreal radius = theme::nodeRadius();
    painter.setOpacity(node.bypass ? 0.5 : 1.0);
    // The body, then the header clipped to the top corners: filled with the category color, or
    // (a tinted theme) a wash of it under an accent bar, as the Maya Graph Editor draws it.
    QPainterPath body;
    body.addRoundedRect(rect, radius, radius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::nodeBody());
    painter.drawPath(body);
    painter.save();
    painter.setClipPath(body);
    if (theme::tintedHeaders()) {
        QColor wash = color;
        wash.setAlpha(34);
        painter.setBrush(wash);
        painter.drawRect(QRectF(rect.topLeft(), QSizeF(rect.width(), kHeader)));
        painter.setBrush(color);
        painter.drawRect(QRectF(rect.topLeft(), QSizeF(rect.width(), 3.0)));
    } else {
        painter.setBrush(color);
        painter.drawRect(QRectF(rect.topLeft(), QSizeF(rect.width(), kHeader)));
    }
    painter.restore();
    // A thin border where the theme gives the nodes one (its color other than the body's).
    if (const QColor border = theme::nodeBorder(); border.isValid() && border != theme::nodeBody()) {
        painter.setPen(QPen(border, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius - 0.5, radius - 0.5);
    }
    // The name, with the type after it when the name says something else.
    painter.setFont(theme::uiFont(13, 800));
    painter.setPen(headerText(color));
    const QRectF title(rect.x() + 12, rect.y(), rect.width() - 40, kHeader);
    painter.drawText(title, Qt::AlignVCenter | Qt::AlignLeft, QString::fromStdString(node.name));
    // The node's time in its last run, before the bypass toggle (a tenth of a millisecond and up).
    if (const auto t = times_.find(node.name); t != times_.end() && t->second >= 0.05) {
        painter.setFont(theme::monoFont(9));
        QColor dim = headerText(color);
        dim.setAlpha(170);
        painter.setPen(dim);
        const QString ms = t->second < 10.0 ? QString::number(t->second, 'f', 1) : QString::number(std::lround(t->second));
        painter.drawText(QRectF(rect.x(), rect.y(), rect.width() - 30, kHeader), Qt::AlignVCenter | Qt::AlignRight, ms + QStringLiteral(" ms"));
    }
    // A note (the node does nothing as set): a grey "i" on the node's corner (left of the error
    // badge when the node failed too).
    if (notes_.count(node.name)) {
        const QPointF corner(rect.right() - (failed ? 20.0 : 0.0), rect.top());
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::graphNote());
        painter.drawEllipse(corner, 8.0, 8.0);
        painter.setFont(theme::uiFont(10, 800));
        painter.setPen(theme::readableOn(theme::graphNote()));
        painter.drawText(QRectF(corner.x() - 8.0, corner.y() - 8.0, 16.0, 16.0), Qt::AlignCenter, QStringLiteral("i"));
    }
    // When it runs, unless always: a tag under the card.
    if (node.evaluation == "viewportOnly" || node.evaluation == "renderOnly") {
        painter.setFont(theme::uiFont(10, 600));
        painter.setPen(theme::textMuted());
        painter.drawText(QRectF(rect.x() + 4, rect.bottom() + 2, rect.width() - 8, 16), Qt::AlignLeft | Qt::AlignVCenter,
                         node.evaluation == "viewportOnly" ? tr("viewport only") : tr("render only"));
    }
    // The bypass toggle in the header's corner.
    const QRectF bypass = bypassButtonRect(node);
    painter.setPen(QPen(headerText(color), 1.6));
    painter.setBrush(node.bypass ? headerText(color) : Qt::NoBrush);
    painter.drawEllipse(bypass.adjusted(3, 3, -3, -3));
    // The ports: inputs down the left edge, outputs down the right, named inside.
    painter.setFont(theme::uiFont(11, 600));
    for (std::size_t i = 0; i < node.inputs.size(); ++i) {
        const QPointF at = inputPos(node, static_cast<int>(i));
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::portColor(node.inputs[i].type));
        painter.drawEllipse(at, kPortRadius, kPortRadius);
        painter.setPen(node.inputs[i].optional ? theme::textMuted() : theme::textDim());
        painter.drawText(QRectF(at.x() + 10, at.y() - kRow / 2, rect.width() / 2, kRow), Qt::AlignVCenter | Qt::AlignLeft,
                         QString::fromStdString(node.inputs[i].name));
    }
    for (std::size_t i = 0; i < node.outputs.size(); ++i) {
        const QPointF at = outputPos(node, static_cast<int>(i));
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::portColor(node.outputs[i].type));
        painter.drawEllipse(at, kPortRadius, kPortRadius);
        painter.setPen(theme::textDim());
        painter.drawText(QRectF(at.x() - 10 - rect.width() / 2, at.y() - kRow / 2, rect.width() / 2, kRow), Qt::AlignVCenter | Qt::AlignRight,
                         QString::fromStdString(node.outputs[i].name));
    }
    if (node.type != node.name && node.inputs.size() <= 1 && node.outputs.size() <= 1) {
        // Room under the ports for the type, dim.
        painter.setPen(theme::textMuted());
        painter.setFont(theme::uiFont(10, 600));
        painter.drawText(QRectF(rect.x(), rect.bottom() - 16, rect.width(), 14), Qt::AlignCenter, QString::fromStdString(nodeLabel(node.type)));
    }
    painter.setOpacity(1.0);
    // Failed (its evaluation threw): a red ring and a red "!" on the corner, as Maya's error badge.
    if (failed) {
        painter.setPen(QPen(theme::graphError(), 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect.adjusted(-3, -3, 3, 3), radius + 3, radius + 3);
        const QPointF corner(rect.right(), rect.top());
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::graphError());
        painter.drawEllipse(corner, 9.0, 9.0);
        painter.setFont(theme::uiFont(12, 900));
        painter.setPen(theme::readableOn(theme::graphError()));
        painter.drawText(QRectF(corner.x() - 9.0, corner.y() - 9.0, 18.0, 18.0), Qt::AlignCenter, QStringLiteral("!"));
    }
    // Viewed (View Up to Here): a tag over the card, the viewport shows the graph up to here.
    if (node.name == viewed_) {
        painter.setFont(theme::uiFont(10, 800));
        const QString tag = tr("VIEWING");
        const qreal w = QFontMetricsF(painter.font()).horizontalAdvance(tag) + 14.0;
        const QRectF pill(rect.x() + 10, rect.y() - 20, w, 16);
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::blueLight());
        painter.drawRoundedRect(pill, 8, 8);
        painter.setPen(theme::readableOn(theme::blueLight()));
        painter.drawText(pill, Qt::AlignCenter, tag);
        painter.setPen(QPen(theme::blueLight(), 1.5, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect.adjusted(-5, -5, 5, 5), radius + 5, radius + 5);
    }
    if (selected) {
        painter.setPen(QPen(theme::nodeSelected(), node.name == selected_ ? 2.5 : 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect.adjusted(-1, -1, 1, 1), radius + 1, radius + 1);
    }
}

void NodeGraphView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    // The viewport dims behind the graph, so the cards and wires read over a busy groom.
    QColor dim = theme::backdrop();
    dim.setAlpha(110);
    painter.fillRect(rect(), dim);
    painter.save();
    painter.translate(pan_);
    painter.scale(scale_, scale_);
    // The backdrops first: a tinted frame with its title bar and resize grip (as Maya's), the
    // groom still showing through; the selected one ringed.
    for (std::size_t i = 0; i < backdrops_.size(); ++i) {
        const Backdrop& b = backdrops_[i];
        const bool selected = static_cast<int>(i) == selectedBackdrop_;
        const qreal radius = theme::nodeRadius();
        QColor fill = b.color;
        fill.setAlpha(46);
        painter.setPen(selected ? QPen(theme::nodeSelected(), 2.0) : QPen(QColor(b.color.red(), b.color.green(), b.color.blue(), 120), 1.5));
        painter.setBrush(fill);
        painter.drawRoundedRect(b.rect, radius, radius);
        QPainterPath frame;
        frame.addRoundedRect(b.rect, radius, radius);
        painter.save();
        painter.setClipPath(frame);
        QColor header = b.color;
        header.setAlpha(120);
        painter.fillRect(QRectF(b.rect.topLeft(), QSizeF(b.rect.width(), kBackdropHeader)), header);
        painter.restore();
        painter.setFont(theme::uiFont(14, 800));
        painter.setPen(theme::textSoft());
        const QRectF title(b.rect.left() + 12, b.rect.top(), b.rect.width() - 24, kBackdropHeader);
        painter.drawText(title, Qt::AlignVCenter | Qt::AlignLeft, QFontMetricsF(painter.font()).elidedText(b.title, Qt::ElideRight, title.width()));
        painter.setPen(QPen(b.color.lighter(130), 1.5));
        for (const qreal k : {4.0, 8.0, 12.0}) {
            painter.drawLine(QPointF(b.rect.right() - k, b.rect.bottom() - 2), QPointF(b.rect.right() - 2, b.rect.bottom() - k));
        }
    }
    // The wires, the one being dragged with them, then the nodes, the selected one last.
    for (std::size_t i = 0; i < wires_.size(); ++i) {
        const Wire& w = wires_[i];
        if (dragWireInputNode_ == w.to && dragWireInputPort_ == w.input && drag_ == Drag::Wire) continue;  // picked up
        const bool selected = static_cast<int>(i) == selectedWire_;
        QColor color = theme::portColor(nodes_[w.from].outputs[w.output].type);
        color.setAlpha(selected ? 255 : 220);
        painter.setPen(QPen(selected ? theme::nodeSelected() : color, selected ? 3.0 : 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(w.path);
    }
    if (drag_ == Drag::Wire && dragWireFrom_ >= 0) {
        QColor color = theme::portColor(nodes_[dragWireFrom_].outputs[dragWirePort_].type);
        painter.setPen(QPen(color, 2.0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(wirePath(outputPos(nodes_[dragWireFrom_], dragWirePort_), dragWireTo_));
    }
    // The nodes, the selected ones last (the current one on top).
    for (const Node& n : nodes_) {
        if (!isSelected(n.name)) drawNode(painter, n, false);
    }
    for (const Node& n : nodes_) {
        if (isSelected(n.name) && n.name != selected_) drawNode(painter, n, true);
    }
    if (const int current = nodeIndex(selected_); current >= 0) drawNode(painter, nodes_[current], true);
    painter.restore();
    // The rubber band.
    if (drag_ == Drag::Band && !band_.isNull()) {
        QColor fill = theme::nodeSelected();
        fill.setAlpha(28);
        painter.setPen(QPen(theme::nodeSelected(), 1.0, Qt::DashLine));
        painter.setBrush(fill);
        painter.drawRect(band_.normalized());
    }
    // Why the dragged wire cannot connect here, by the cursor.
    if (drag_ == Drag::Wire && !wireHint_.isEmpty()) {
        painter.setFont(theme::uiFont(12, 700));
        const QFontMetricsF metrics(painter.font());
        const QPointF at = toWidget(dragWireTo_) + QPointF(14, 14);
        QRectF box(at, QSizeF(std::min(360.0, metrics.horizontalAdvance(wireHint_) + 20.0), 26.0));
        if (box.right() > width() - 8) box.moveRight(width() - 8);
        QColor fill = theme::glassSolid();
        fill.setAlpha(235);
        painter.setPen(QPen(theme::graphError(), 1.2));
        painter.setBrush(fill);
        painter.drawRoundedRect(box, 8, 8);
        painter.setPen(theme::errorText());
        painter.drawText(box.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
                         metrics.elidedText(wireHint_, Qt::ElideRight, box.width() - 20));
    }
}

// ---- Mouse ---------------------------------------------------------------------------------------

void NodeGraphView::forwardToViewport(QEvent* event) {
    // The graph covers the viewport exactly, so the positions carry over as they are.
    if (viewport_) QCoreApplication::sendEvent(viewport_, event);
}

void NodeGraphView::mousePressEvent(QMouseEvent* event) {
    setFocus();
    if (searchOpen()) closeSearch();
    if (promptOpen()) {
        prompt_->hide();
        promptDone_ = nullptr;
    }
    last_ = event->position();
    pressScene_ = toScene(last_);
    if (event->modifiers().testFlag(Qt::AltModifier)) {
        drag_ = Drag::Camera;
        forwardToViewport(event);
        return;
    }
    event->accept();
    if (event->button() == Qt::MiddleButton) {
        drag_ = Drag::Pan;
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    int node = -1, port = -1;
    bool output = false;
    if (bypassButtonAt(pressScene_, &node)) {
        setBypass(nodes_[node].name, !nodes_[node].bypass);
        return;
    }
    if (portAt(pressScene_, &node, &port, &output)) {
        if (output) {
            drag_ = Drag::Wire;
            dragWireFrom_ = node;
            dragWirePort_ = port;
            dragWireInputNode_ = dragWireInputPort_ = -1;
            dragWireTo_ = pressScene_;
        } else {
            // A fed input: its wire is picked up from its source end.
            for (const Wire& w : wires_) {
                if (w.to == node && w.input == port) {
                    drag_ = Drag::Wire;
                    dragWireFrom_ = w.from;
                    dragWirePort_ = w.output;
                    dragWireInputNode_ = node;
                    dragWireInputPort_ = port;
                    dragWireTo_ = pressScene_;
                    break;
                }
            }
        }
        update();
        return;
    }
    node = nodeAt(pressScene_);
    if (node >= 0) {
        const std::string& name = nodes_[node].name;
        const Qt::KeyboardModifiers mods = event->modifiers();
        if (mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::ShiftModifier)) {
            // Ctrl toggles the node in the selection, Shift adds it; no move.
            std::vector<std::string> names = selection_;
            const auto it = std::find(names.begin(), names.end(), name);
            if (it != names.end() && mods.testFlag(Qt::ControlModifier)) names.erase(it);
            else if (it == names.end()) names.push_back(name);
            else {
                names.erase(it);
                names.push_back(name);  // Shift on a selected node: it becomes the current one
            }
            setSelection(names);
            return;
        }
        drag_ = Drag::Move;
        dragNode_ = node;
        moveStart_ = nodes_[node].pos;
        // A node of the selection drags the whole selection; another one is selected alone.
        if (isSelected(name)) {
            std::vector<std::string> names = selection_;
            names.erase(std::find(names.begin(), names.end(), name));
            names.push_back(name);
            setSelection(names);
        } else {
            selectNode(name);
        }
        return;
    }
    // A backdrop's title bar: selected and moved with the nodes on it; its corner: resized.
    bool grip = false;
    if (const int b = backdropTitleAt(pressScene_, &grip); b >= 0) {
        selectBackdrop(b);
        drag_ = grip ? Drag::BackdropResize : Drag::Backdrop;
        dragBackdrop_ = b;
        backdropStart_ = backdrops_[b].rect;
        carried_.clear();
        if (!grip) {
            for (std::size_t i = 0; i < nodes_.size(); ++i) {
                if (backdropStart_.contains(QRectF(nodes_[i].pos, nodes_[i].size).center())) carried_.push_back(static_cast<int>(i));
            }
        }
        return;
    }
    const int wire = wireAt(pressScene_);
    if (wire >= 0) {
        selectedWire_ = wire;
        selectedBackdrop_ = -1;
        selection_.clear();
        if (!selected_.empty()) {
            selected_.clear();
            emit selectionChanged(selected_);
        }
        update();
        return;
    }
    // Empty space: a rubber band (a click alone deselects, unless Ctrl or Shift keeps the selection).
    drag_ = Drag::Band;
    band_ = QRectF(last_, last_);
    bandAdds_ = event->modifiers().testFlag(Qt::ControlModifier) || event->modifiers().testFlag(Qt::ShiftModifier);
    selectedWire_ = -1;
    if (!bandAdds_) selectNode({});
    update();
}

void NodeGraphView::mouseMoveEvent(QMouseEvent* event) {
    const QPointF now = event->position();
    const QPointF delta = now - last_;
    last_ = now;
    switch (drag_) {
        case Drag::Camera:
            forwardToViewport(event);
            return;
        case Drag::Pan:
            pan_ += delta;
            update();
            break;
        case Drag::Move:
            if (dragNode_ >= 0) {
                // The selection moves together (the pressed node is one of it); the wires follow.
                std::vector<int> moved;
                for (std::size_t i = 0; i < nodes_.size(); ++i) {
                    if (static_cast<int>(i) == dragNode_ || isSelected(nodes_[i].name)) moved.push_back(static_cast<int>(i));
                }
                for (int i : moved) nodes_[i].pos += delta / scale_;
                for (Wire& w : wires_) {
                    if (std::find(moved.begin(), moved.end(), w.from) != moved.end() || std::find(moved.begin(), moved.end(), w.to) != moved.end()) {
                        w.path = wirePathFor(w);
                    }
                }
                update();
            }
            break;
        case Drag::Wire:
            dragWireTo_ = toScene(now);
            updateWireHint(dragWireTo_);
            update();
            break;
        case Drag::Band:
            band_.setBottomRight(now);
            update();
            break;
        case Drag::Backdrop:
        case Drag::BackdropResize:
            if (dragBackdrop_ >= 0 && dragBackdrop_ < static_cast<int>(backdrops_.size())) {
                const QPointF moved = toScene(now) - pressScene_;
                QRectF& rect = backdrops_[dragBackdrop_].rect;
                if (drag_ == Drag::BackdropResize) {
                    rect.setSize(QSizeF(std::max(120.0, backdropStart_.width() + moved.x()), std::max(80.0, backdropStart_.height() + moved.y())));
                } else {
                    const QPointF step = backdropStart_.topLeft() + moved - rect.topLeft();
                    rect.moveTopLeft(backdropStart_.topLeft() + moved);
                    for (int i : carried_) nodes_[i].pos += step;
                    for (Wire& w : wires_) {
                        if (std::find(carried_.begin(), carried_.end(), w.from) != carried_.end() ||
                            std::find(carried_.begin(), carried_.end(), w.to) != carried_.end()) {
                            w.path = wirePathFor(w);
                        }
                    }
                }
                update();
            }
            break;
        case Drag::None:
            break;
    }
    event->accept();
}

void NodeGraphView::mouseReleaseEvent(QMouseEvent* event) {
    const Drag drag = drag_;
    drag_ = Drag::None;
    if (drag == Drag::Camera) {
        forwardToViewport(event);
        return;
    }
    event->accept();
    if (drag == Drag::Band) {
        band_.setBottomRight(event->position());
        if (band_.width() * band_.width() + band_.height() * band_.height() > 16.0) selectInRect(band_, bandAdds_);
        band_ = QRectF();
        update();
        return;
    }
    if ((drag == Drag::Backdrop || drag == Drag::BackdropResize) && dragBackdrop_ >= 0 && document_) {
        // Where it was put, and the nodes it carried (not undo steps, as node moves).
        if (dragBackdrop_ < static_cast<int>(backdrops_.size()) && backdrops_[dragBackdrop_].rect != backdropStart_) {
            const QRectF& r = backdrops_[dragBackdrop_].rect;
            document_->setBackdropRect(dragBackdrop_, r.x(), r.y(), r.width(), r.height());
            for (int i : carried_) {
                document_->setNodePosition(nodes_[i].name, static_cast<float>(nodes_[i].pos.x()), static_cast<float>(nodes_[i].pos.y()));
                laidOut_.erase(nodes_[i].name);
            }
        }
        dragBackdrop_ = -1;
        carried_.clear();
        return;
    }
    if (drag == Drag::Move && dragNode_ >= 0 && document_) {
        // Every moved node keeps where it was put (not an undo step, as Maya's moves).
        if ((nodes_[dragNode_].pos - moveStart_).manhattanLength() > 0.0) {
            for (const Node& n : nodes_) {
                if (n.name != nodes_[dragNode_].name && !isSelected(n.name)) continue;
                document_->setNodePosition(n.name, static_cast<float>(n.pos.x()), static_cast<float>(n.pos.y()));
                laidOut_.erase(n.name);
            }
        }
        dragNode_ = -1;
        return;
    }
    if (drag == Drag::Wire && document_) {
        const QPointF at = toScene(event->position());
        int node = -1, port = -1;
        bool output = false;
        std::string error;
        if (portAt(at, &node, &port, &output)) {
            if (output || (dragWireInputNode_ == node && dragWireInputPort_ == port)) {
                // On an output, or back where it was: nothing changes (a picked-up wire goes back).
                if (output && node != dragWireFrom_) error = tr("Drop the wire on an input (the ports on a node's left)").toStdString();
            } else if (const QString problem = connectionProblem(dragWireFrom_, dragWirePort_, node, port); !problem.isEmpty()) {
                error = problem.toStdString();  // refused: the wire as it was
            } else {
                const Node& from = nodes_[dragWireFrom_];
                const Node& to = nodes_[node];
                const bool first = dragWirePort_ == 0;
                error = document_->connectNodes(from.name, first ? std::string() : from.outputs[dragWirePort_].name, to.name,
                                                to.inputs[port].name);
                if (error.empty()) emit edited();
            }
        } else if (nodeAt(at) >= 0) {
            // On a card but no port: nothing changes (a picked-up wire is not lost by a near miss).
            if (dragWireInputNode_ < 0) error = tr("Drop the wire on an input port").toStdString();
        } else if (dragWireInputNode_ >= 0) {
            error = document_->disconnectInput(nodes_[dragWireInputNode_].name, nodes_[dragWireInputNode_].inputs[dragWireInputPort_].name);
            if (error.empty()) emit edited();
        }
        if (!error.empty()) emit message(QString::fromStdString(error));
        dragWireFrom_ = dragWirePort_ = dragWireInputNode_ = dragWireInputPort_ = -1;
        wireHint_.clear();
        refresh();
    }
}

QString NodeGraphView::connectionProblem(int fromNode, int fromPort, int toNode, int toPort) const {
    if (fromNode < 0 || toNode < 0 || fromNode >= nodeCount() || toNode >= nodeCount()) return {};
    const Node& from = nodes_[fromNode];
    const Node& to = nodes_[toNode];
    if (fromPort < 0 || fromPort >= static_cast<int>(from.outputs.size()) || toPort < 0 || toPort >= static_cast<int>(to.inputs.size())) return {};
    if (fromNode == toNode) return tr("A node can't feed itself");
    const Port& out = from.outputs[fromPort];
    const Port& in = to.inputs[toPort];
    try {
        if (!portTypesMatch(portTypeFromName(out.type), portTypeFromName(in.type))) {
            return tr("%1.%2 takes %3, %4 gives %5")
                .arg(QString::fromStdString(to.name), QString::fromStdString(in.name), QString::fromStdString(in.type).toLower(),
                     QString::fromStdString(from.name), QString::fromStdString(out.type).toLower());
        }
    } catch (const std::exception&) {
        // A type the core does not name: the document's own checks say.
    }
    // A loop: the target feeds the source already (directly or not).
    std::vector<int> stack{toNode};
    std::vector<bool> seen(nodes_.size(), false);
    while (!stack.empty()) {
        const int at = stack.back();
        stack.pop_back();
        if (at == fromNode) return tr("That would make a loop: %1 feeds %2 already").arg(QString::fromStdString(to.name), QString::fromStdString(from.name));
        if (seen[at]) continue;
        seen[at] = true;
        for (const Wire& w : wires_) {
            if (w.from == at) stack.push_back(w.to);
        }
    }
    return {};
}

void NodeGraphView::updateWireHint(const QPointF& scene) {
    int node = -1, port = -1;
    bool output = false;
    wireHint_.clear();
    if (dragWireFrom_ < 0 || !portAt(scene, &node, &port, &output)) return;
    if (output) {
        if (node != dragWireFrom_) wireHint_ = tr("Outputs don't connect to outputs: drop it on an input");
        return;
    }
    wireHint_ = connectionProblem(dragWireFrom_, dragWirePort_, node, port);
}

void NodeGraphView::wheelEvent(QWheelEvent* event) {
    if (event->modifiers().testFlag(Qt::AltModifier)) {
        forwardToViewport(event);
        return;
    }
    // Zoom about the cursor: the scene point under it stays put.
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0) return;
    const qreal factor = std::pow(1.15, steps);
    const qreal next = std::clamp(scale_ * factor, 0.25, 2.5);
    const QPointF at = event->position();
    const QPointF scene = toScene(at);
    scale_ = next;
    pan_ = at - scene * scale_;
    update();
    event->accept();
}

// ---- Keys and the search --------------------------------------------------------------------------

void NodeGraphView::setBypass(const std::string& node, bool bypass) {
    if (!document_) return;
    const std::string error = document_->setBypass(node, bypass);
    if (!error.empty()) {
        emit message(QString::fromStdString(error));
        return;
    }
    emit edited();
    refresh();
}

void NodeGraphView::setEvaluation(const std::string& node, const std::string& mode) {
    if (!document_) return;
    const std::string error = document_->setEvaluation(node, mode);
    if (!error.empty()) {
        emit message(QString::fromStdString(error));
        return;
    }
    emit edited();
    refresh();
}

void NodeGraphView::setNodeTimes(const std::vector<std::pair<std::string, double>>& times) {
    times_.clear();
    for (const auto& [name, ms] : times) times_[name] = ms;
    update();
}

void NodeGraphView::setNodeErrors(const std::vector<std::pair<std::string, std::string>>& errors) {
    errors_.clear();
    for (const auto& [name, error] : errors) errors_[name] = error;
    update();
}

std::string NodeGraphView::nodeError(const std::string& node) const {
    const auto it = errors_.find(node);
    return it == errors_.end() ? std::string() : it->second;
}

void NodeGraphView::setViewedNode(const std::string& node) {
    if (node == viewed_) return;
    viewed_ = node;
    update();
    emit viewNodeChanged(viewed_);
}

void NodeGraphView::toggleViewSelected() {
    // V: the selected node shown; again (or with nothing selected), the output.
    if (selected_.empty() || selected_ == viewed_) {
        if (!viewed_.empty()) emit message(tr("Viewing the graph's output again"));
        setViewedNode({});
        return;
    }
    const int i = nodeIndex(selected_);
    if (i < 0) return;
    const bool strands = std::any_of(nodes_[i].outputs.begin(), nodes_[i].outputs.end(), [](const Port& p) {
        return p.type == "strands" || p.type == "Strands" || p.type == "curves" || p.type == "Curves";
    });
    if (!strands) {
        emit message(tr("%1 gives no strands to view: pick a node with a strands output").arg(QString::fromStdString(selected_)));
        return;
    }
    setViewedNode(selected_);
    emit message(tr("Viewing up to %1 (V again: the output)").arg(QString::fromStdString(selected_)));
}

void NodeGraphView::copySelected() {
    if (!document_) return;
    std::vector<std::string> names = selection_;
    if (names.empty() && !selected_.empty()) names.push_back(selected_);
    const std::string text = document_->copyNodes(names);
    if (text.empty()) {
        emit message(tr("Select the nodes to copy (the output node is one of a kind)"));
        return;
    }
    QGuiApplication::clipboard()->setText(QString::fromStdString(text));
    emit message(tr("Copied %n node(s)", "", static_cast<int>(nlohmann::json::parse(text)["nodes"].size())));
}

void NodeGraphView::paste() {
    if (!document_) return;
    std::vector<std::string> made;
    const std::string error = document_->pasteNodes(QGuiApplication::clipboard()->text().toStdString(), static_cast<float>(kPasteOffsetX),
                                                    static_cast<float>(kPasteOffsetY), &made);
    if (!error.empty()) {
        emit message(QString::fromStdString(error));
        return;
    }
    emit edited();
    refresh();
    setSelection(made);
}

void NodeGraphView::deleteSelected() {
    if (!document_) return;
    std::string error;
    if (selectedBackdrop_ >= 0) {
        removeBackdrop(selectedBackdrop_);
        return;
    }
    if (selectedWire_ >= 0 && selectedWire_ < static_cast<int>(wires_.size())) {
        const Wire& w = wires_[selectedWire_];
        error = document_->disconnectInput(nodes_[w.to].name, nodes_[w.to].inputs[w.input].name);
        selectedWire_ = -1;
    } else if (!selection_.empty()) {
        error = document_->removeNodes(selection_);
    } else {
        return;
    }
    if (!error.empty()) emit message(QString::fromStdString(error));
    else emit edited();
    refresh();
}

bool NodeGraphView::event(QEvent* event) {
    // A node's tooltip: its error, else its note (the badges' text).
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        const int node = nodeAt(toScene(QPointF(help->pos())));
        QString tip;
        if (node >= 0) {
            const std::string error = nodeError(nodes_[node].name), note = nodeNote(nodes_[node].name);
            tip = !error.empty() ? QString::fromStdString(error) : QString::fromStdString(note);
        }
        if (tip.isEmpty()) QToolTip::hideText();
        else QToolTip::showText(help->globalPos(), tip, this);
        return true;
    }
    return QWidget::event(event);
}

void NodeGraphView::setNodeNotes(const std::vector<std::pair<std::string, std::string>>& notes) {
    notes_.clear();
    for (const auto& [name, note] : notes) notes_[name] = note;
    update();
}

std::string NodeGraphView::nodeNote(const std::string& node) const {
    const auto it = notes_.find(node);
    return it == notes_.end() ? std::string() : it->second;
}

// ---- The edits a node's menu offers --------------------------------------------------------------

void NodeGraphView::renameSelected() {
    if (!document_ || selected_.empty()) return;
    const std::string name = selected_;
    promptName(tr("Rename %1").arg(QString::fromStdString(name)), tr("Node name"), [this, name](const QString& to) {
        const std::string error = document_->renameNode(name, to.trimmed().toStdString());
        if (!error.empty()) {
            emit message(QString::fromStdString(error));
            return;
        }
        selected_ = to.trimmed().toStdString();
        selection_ = {selected_};
        emit edited();
        refresh();
        emit selectionChanged(selected_);
    });
    setPromptText(QString::fromStdString(name));
}

void NodeGraphView::duplicateSelected() {
    if (!document_ || selected_.empty()) return;
    if (selection_.size() > 1) {
        // Several: copied and pasted beside, with the links between them (not the clipboard's).
        std::vector<std::string> pasted;
        const std::string text = document_->copyNodes(selection_);
        const std::string error = text.empty() ? std::string("the output node is one of a kind")
                                               : document_->pasteNodes(text, static_cast<float>(kPasteOffsetX), static_cast<float>(kPasteOffsetY), &pasted);
        if (!error.empty()) {
            emit message(QString::fromStdString(error));
            return;
        }
        emit edited();
        refresh();
        setSelection(pasted);
        return;
    }
    std::string made;
    const std::string error = document_->duplicateNode(selected_, &made);
    if (!error.empty()) {
        emit message(QString::fromStdString(error));
        return;
    }
    emit edited();
    refresh();
    selectNode(made);
}

void NodeGraphView::setSelectedAsOutput() {
    if (!document_ || selected_.empty()) return;
    const std::string error = document_->setOutputNode(selected_);
    if (!error.empty()) {
        emit message(QString::fromStdString(error));
        return;
    }
    emit edited();
    refresh();
}

void NodeGraphView::tidyLayout() {
    if (!document_) return;
    const std::string error = document_->tidyLayout();
    if (!error.empty()) {
        emit message(QString::fromStdString(error));
        return;
    }
    laidOut_.clear();
    fitted_ = false;
    emit edited();
    refresh();
}

void NodeGraphView::groupSelected() {
    if (!document_) return;
    std::vector<std::string> names = selection_;
    if (names.empty() && !selected_.empty()) names.push_back(selected_);
    if (names.empty()) {
        emit message(tr("Select the nodes to group"));
        return;
    }
    // Nodes the view laid out keep where they show, so the compound goes among them and opening
    // it puts them back there.
    for (const std::string& n : names) {
        const int i = nodeIndex(n);
        float x = 0.0f, y = 0.0f;
        if (i >= 0 && !document_->nodePosition(n, &x, &y)) {
            document_->setNodePosition(n, static_cast<float>(nodes_[i].pos.x()), static_cast<float>(nodes_[i].pos.y()));
        }
    }
    std::string made;
    const std::string error = document_->collapseNodes(names, &made);
    if (!error.empty()) {
        emit message(QString::fromStdString(error));
        return;
    }
    emit edited();
    refresh();
    selectNode(made);
    emit message(tr("Grouped %n node(s) into %1 (double-click or Ctrl+Shift+G opens it)", "", static_cast<int>(names.size()))
                     .arg(QString::fromStdString(made)));
}

void NodeGraphView::ungroupSelected() {
    if (!document_) return;
    std::vector<std::string> compounds;
    for (const std::string& n : selection_) {
        const int i = nodeIndex(n);
        if (i >= 0 && nodes_[i].type == "Compound") compounds.push_back(n);
    }
    if (compounds.empty()) {
        emit message(tr("Select the Compound nodes to ungroup"));
        return;
    }
    std::vector<std::string> made;
    const std::string error = document_->expandNodes(compounds, &made);
    if (!error.empty()) {
        emit message(QString::fromStdString(error));
        return;
    }
    emit edited();
    refresh();
    setSelection(made);
}

void NodeGraphView::addBackdrop(const QPointF& at) {
    if (!document_) return;
    // Around the selected nodes (Maya's margins: room for the title above), else a frame at the point.
    QRectF rect;
    for (const std::string& n : selection_) {
        const int i = nodeIndex(n);
        if (i >= 0) rect |= QRectF(nodes_[i].pos, nodes_[i].size);
    }
    if (!rect.isNull()) {
        rect.adjust(-30, -50, 30, 30);
    } else {
        const QPointF center = toScene(std::isnan(at.x()) ? QPointF(width() / 2.0, height() / 2.0) : at);
        rect = QRectF(center.x() - 200, center.y() - 120, 400, 240);
    }
    int index = -1;
    const std::string error = document_->addBackdrop("Backdrop", rect.x(), rect.y(), rect.width(), rect.height(), &index);
    if (!error.empty()) {
        emit message(QString::fromStdString(error));
        return;
    }
    refresh();
    selectBackdrop(index);
    renameBackdrop(index);  // named at once (Escape keeps "Backdrop")
}

void NodeGraphView::selectBackdrop(int index) {
    selectNode({});
    selectedWire_ = -1;
    selectedBackdrop_ = index >= 0 && index < backdropCount() ? index : -1;
    update();
}

QString NodeGraphView::backdropTitle(int index) const { return index >= 0 && index < backdropCount() ? backdrops_[index].title : QString(); }

QColor NodeGraphView::backdropColor(int index) const { return index >= 0 && index < backdropCount() ? backdrops_[index].color : QColor(); }

QRectF NodeGraphView::backdropRect(int index) const { return index >= 0 && index < backdropCount() ? backdrops_[index].rect : QRectF(); }

QRectF NodeGraphView::backdropTitleBar(int index) const {
    if (index < 0 || index >= backdropCount()) return {};
    const QRectF& r = backdrops_[index].rect;
    return QRectF(toWidget(r.topLeft()), QSizeF(r.width(), kBackdropHeader) * scale_);
}

QRectF NodeGraphView::backdropGrip(int index) const {
    if (index < 0 || index >= backdropCount()) return {};
    const QRectF& r = backdrops_[index].rect;
    return QRectF(toWidget(QPointF(r.right() - kBackdropGrip, r.bottom() - kBackdropGrip)), QSizeF(kBackdropGrip, kBackdropGrip) * scale_);
}

void NodeGraphView::renameBackdrop(int index) {
    if (!document_ || index < 0 || index >= backdropCount()) return;
    promptName(tr("Backdrop title"), tr("Title"), [this, index](const QString& title) {
        const std::string error = document_->setBackdropTitle(index, title.toStdString());
        if (!error.empty()) emit message(QString::fromStdString(error));
        refresh();
    });
    setPromptText(backdrops_[index].title);
}

void NodeGraphView::setBackdropColor(int index, const QColor& color) {
    if (!document_) return;
    const std::string error = document_->setBackdropColor(index, color.red(), color.green(), color.blue());
    if (!error.empty()) emit message(QString::fromStdString(error));
    refresh();
}

void NodeGraphView::removeBackdrop(int index) {
    if (!document_) return;
    const std::string error = document_->removeBackdrop(index);
    if (!error.empty()) emit message(QString::fromStdString(error));
    selectedBackdrop_ = -1;
    refresh();
}

void NodeGraphView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || event->modifiers().testFlag(Qt::AltModifier)) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    const QPointF scene = toScene(event->position());
    const int node = nodeAt(scene);
    if (node >= 0 && nodes_[node].type == "Compound") {
        // A Compound opens: its nodes back in the graph to see and edit (Ctrl+Z groups them again).
        drag_ = Drag::None;
        selectNode(nodes_[node].name);
        ungroupSelected();
    } else if (node >= 0 && scene.y() - nodes_[node].pos.y() <= kHeader) {
        drag_ = Drag::None;
        selectNode(nodes_[node].name);
        renameSelected();
    } else if (const int b = node < 0 ? backdropTitleAt(scene) : -1; b >= 0) {
        drag_ = Drag::None;
        renameBackdrop(b);
    }
    event->accept();
}

void NodeGraphView::contextMenuEvent(QContextMenuEvent* event) {
    if (!document_) return;
    const QPointF scene = toScene(QPointF(event->pos()));
    const int node = nodeAt(scene);
    QMenu menu(this);
    if (node >= 0) {
        // A node of the selection keeps the selection (Delete and Copy act on it all).
        if (!isSelected(nodes_[node].name)) selectNode(nodes_[node].name);
        else if (selected_ != nodes_[node].name) {
            std::vector<std::string> names = selection_;
            names.erase(std::find(names.begin(), names.end(), nodes_[node].name));
            names.push_back(nodes_[node].name);
            setSelection(names);
        }
        const bool output = nodes_[node].type == "Output" && selection_.size() <= 1;
        // Each with its key shown (the graph's keyPressEvent handles them; shown here only).
        const auto shown = [](QAction* action, const QKeySequence& key) {
            action->setShortcut(key);
            action->setShortcutContext(Qt::WidgetShortcut);  // (the menu's own: never the window's)
            return action;
        };
        shown(menu.addAction(tr("Rename..."), this, [this] { renameSelected(); }), QKeySequence(Qt::Key_F2));
        shown(menu.addAction(tr("Duplicate"), this, [this] { duplicateSelected(); }), QKeySequence(Qt::CTRL | Qt::Key_D))->setEnabled(!output);
        menu.addAction(tr("Set as Output"), this, [this] { setSelectedAsOutput(); });
        // Bypass, checked while it is: again turns the node back on.
        QAction* bypass = shown(menu.addAction(tr("Bypass"), this,
                                               [this, name = nodes_[node].name, on = !nodes_[node].bypass] { setBypass(name, on); }),
                                QKeySequence(Qt::Key_B));
        bypass->setCheckable(true);
        bypass->setChecked(nodes_[node].bypass);
        // View Up to Here (checked while the viewport shows this node).
        QAction* view = shown(menu.addAction(tr("View Up to Here"), this, [this] { toggleViewSelected(); }), QKeySequence(Qt::Key_V));
        view->setCheckable(true);
        view->setChecked(viewed_ == nodes_[node].name);
        shown(menu.addAction(tr("Frame"), this, [this] { frameSelected(); }), QKeySequence(Qt::Key_F));
        shown(menu.addAction(tr("Copy"), this, [this] { copySelected(); }), QKeySequence(Qt::CTRL | Qt::Key_C))->setEnabled(!output || selection_.size() > 1);
        // When it runs: exports skip a viewport-only node, the viewport a render-only one.
        QMenu* evaluate = menu.addMenu(tr("Evaluate"));
        const std::pair<const char*, QString> modes[] = {
            {"always", tr("Always")}, {"viewportOnly", tr("Viewport Only")}, {"renderOnly", tr("Render Only")}};
        for (const auto& [mode, label] : modes) {
            QAction* action = evaluate->addAction(label, this, [this, name = nodes_[node].name, m = std::string(mode)] {
                setEvaluation(name, m);
            });
            action->setCheckable(true);
            action->setChecked(nodes_[node].evaluation == mode);
        }
        menu.addSeparator();
        // Compounds, as Maya's Graph Editor: the selection grouped, a Compound opened.
        const bool anyCompound = std::any_of(selection_.begin(), selection_.end(), [this](const std::string& n) {
            const int i = nodeIndex(n);
            return i >= 0 && nodes_[i].type == "Compound";
        });
        if (anyCompound) {
            shown(menu.addAction(tr("Ungroup Compound"), this, [this] { ungroupSelected(); }), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G))
                ->setToolTip(tr("Put the compound's nodes back in the graph, to see and edit them (Ctrl+Z groups them again)"));
        }
        shown(menu.addAction(tr("Group into Compound"), this, [this] { groupSelected(); }), QKeySequence(Qt::CTRL | Qt::Key_G))
            ->setToolTip(tr("Collapse the selected nodes into one Compound node"));
        menu.addAction(tr("Add Backdrop Around Selection"), this, [this] { addBackdrop(); });
        menu.addSeparator();
        shown(menu.addAction(selection_.size() > 1 ? tr("Delete %1 Nodes").arg(selection_.size()) : tr("Delete"), this,
                             [this] { deleteSelected(); }),
              QKeySequence(Qt::Key_Delete))
            ->setEnabled(!output);
    } else if (const int b = backdropAt(scene); b >= 0) {
        // A backdrop's menu (Maya's): its title, its color, gone.
        selectBackdrop(b);
        menu.addAction(tr("Rename..."), this, [this, b] { renameBackdrop(b); });
        QMenu* colors = menu.addMenu(tr("Color"));
        for (const auto& [label, color] : kBackdropColors) {
            QAction* action = colors->addAction(tr(label), this, [this, b, c = color] { setBackdropColor(b, c); });
            action->setCheckable(true);
            action->setChecked(backdrops_[b].color == color);
        }
        menu.addSeparator();
        QAction* remove = menu.addAction(tr("Delete Backdrop"), this, [this, b] { removeBackdrop(b); });
        remove->setShortcut(QKeySequence(Qt::Key_Delete));
        remove->setShortcutContext(Qt::WidgetShortcut);
    } else {
        const auto shown = [](QAction* action, const QKeySequence& key) {
            action->setShortcut(key);
            action->setShortcutContext(Qt::WidgetShortcut);
        };
        shown(menu.addAction(tr("Add Node..."), this, [this] { openSearch(); }), QKeySequence(Qt::Key_Tab));
        shown(menu.addAction(tr("Paste"), this, [this] { paste(); }), QKeySequence(Qt::CTRL | Qt::Key_V));
        menu.addAction(tr("Add Backdrop"), this, [this, at = QPointF(event->pos())] { addBackdrop(at); });
        shown(menu.addAction(tr("Tidy Layout"), this, [this] { tidyLayout(); }), QKeySequence(Qt::CTRL | Qt::Key_L));
        shown(menu.addAction(tr("Frame All"), this, [this] { fitAll(); }), QKeySequence(Qt::Key_A));
    }
    menu.exec(event->globalPos());
    event->accept();
}

void NodeGraphView::keyPressEvent(QKeyEvent* event) {
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        if (event->key() == Qt::Key_G) {
            if (event->modifiers().testFlag(Qt::ShiftModifier)) ungroupSelected();
            else groupSelected();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_D) {
            duplicateSelected();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_L) {
            tidyLayout();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_C) {
            copySelected();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_V) {
            paste();
            event->accept();
            return;
        }
    }
    switch (event->key()) {
        case Qt::Key_Tab:
            openSearch();
            event->accept();
            return;
        case Qt::Key_Escape:
            if (searchOpen()) closeSearch();
            else emit hideRequested();
            event->accept();
            return;
        case Qt::Key_F:
            frameSelected();  // (the window's F reaches it as a shortcut: View > Frame All)
            event->accept();
            return;
        case Qt::Key_A:
            fitAll();
            event->accept();
            return;
        case Qt::Key_F2:
            if (!selected_.empty()) renameSelected();
            event->accept();
            return;
        case Qt::Key_B: {
            // Every selected node, each to the current node's opposite (as Maya's B).
            const int current = nodeIndex(selected_);
            if (current >= 0) {
                const bool on = !nodes_[current].bypass;
                const std::vector<std::string> names = selection_;
                for (const std::string& n : names) setBypass(n, on);
            }
            event->accept();
            return;
        }
        case Qt::Key_V:
            if (event->modifiers() == Qt::NoModifier) {
                toggleViewSelected();
                event->accept();
                return;
            }
            break;
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            if (!document_ || (selectedWire_ < 0 && selection_.empty() && selectedBackdrop_ < 0)) break;
            deleteSelected();
            event->accept();
            return;
        default:
            break;
    }
    QWidget::keyPressEvent(event);
}

bool NodeGraphView::searchOpen() const { return !search_->isHidden(); }

void NodeGraphView::openSearch() {
    const QPointF at = mapFromGlobal(QCursor::pos());
    openSearchAt(rect().contains(at.toPoint()) ? at : QPointF(width() / 2.0, height() / 2.0));
}

void NodeGraphView::openSearchAt(const QPointF& where) {
    searchScene_ = toScene(where);
    search_->move(std::clamp(static_cast<int>(where.x()), 0, std::max(0, width() - search_->width())),
                  std::clamp(static_cast<int>(where.y()), 0, std::max(0, height() - search_->height())));
    searchEdit_->clear();
    filterSearch({});
    search_->show();
    search_->raise();
    searchEdit_->setFocus();
}

void NodeGraphView::closeSearch() {
    search_->hide();
    setFocus();
}

namespace {

// A param name as words: "guideRadius" reads "guide radius" (Maya's pretty(), lowercase).
QString spacedName(const QString& name) {
    QString out;
    for (int i = 0; i < name.size(); ++i) {
        const QChar ch = name[i];
        if (ch == QLatin1Char('_')) {
            out += QLatin1Char(' ');
            continue;
        }
        if (i > 0 && ch.isUpper() && (name[i - 1].isLower() || name[i - 1].isDigit())) out += QLatin1Char(' ');
        out += ch;
    }
    return out.toLower();
}

bool allIn(const QStringList& words, const QString& text, const QString& orIn) {
    for (const QString& w : words) {
        if (!text.contains(w) && !orIn.contains(w)) return false;
    }
    return true;
}

}  // namespace

int nodeSearchRank(const QString& search, const nlohmann::json& spec) {
    const QString text = search.trimmed().toLower();
    if (text.isEmpty()) return 5;
    const std::string type = spec.value("type", std::string());
    const QString name = QString::fromStdString(type).toLower();
    const std::string label = spec.value("label", std::string());
    const QString shown = QString::fromStdString(label.empty() ? nodeLabel(type) : label).toLower();
    if (name.startsWith(text) || shown.startsWith(text)) return 0;
    if (name.contains(text) || shown.contains(text) || name.contains(QString(text).remove(QLatin1Char(' ')))) return 1;
    const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    // Rank 2's words: the search keywords and the menu category's title ("deform" finds Bend first).
    const QString keywords = (QString::fromStdString(spec.value("keywords", std::string())) + QLatin1Char(' ') +
                              QString::fromStdString(spec.value("categoryTitle", std::string())))
                                 .toLower();
    QString params, details;
    for (const nlohmann::json& p : spec.value("params", nlohmann::json::array())) {
        const QString pname = QString::fromStdString(p.value("name", std::string()));
        params += pname.toLower() + QLatin1Char(' ') + spacedName(pname) + QLatin1Char(' ');
        details += QString::fromStdString(p.value("description", std::string())).toLower() + QLatin1Char(' ');
    }
    const QString about =
        (QString::fromStdString(spec.value("categoryTitle", std::string())) + QLatin1Char(' ') + QString::fromStdString(spec.value("description", std::string())))
            .toLower();
    if (allIn(words, keywords, name)) return 2;
    if (allIn(words, params, name)) return 3;
    if (allIn(words, about, name)) return 4;
    const QString everything = name + QLatin1Char(' ') + keywords + QLatin1Char(' ') + params + QLatin1Char(' ') + about + QLatin1Char(' ') + details;
    if (allIn(words, everything, everything)) return 5;
    return -1;
}

std::vector<std::string> nodeSearchResults(const GroomDocument& document, const QString& text) {
    std::vector<std::tuple<int, int, int, std::string>> ranked;  // rank, category order, order, type
    for (const std::string& type : document.operatorTypes()) {
        const nlohmann::json* spec = document.spec(type);
        if (!spec || spec->value("hidden", false)) continue;
        const int rank = nodeSearchRank(text, *spec);
        if (rank < 0) continue;
        ranked.emplace_back(rank, spec->value("categoryOrder", std::numeric_limits<int>::max()),
                            spec->value("order", std::numeric_limits<int>::max()), type);
    }
    std::sort(ranked.begin(), ranked.end());
    std::vector<std::string> types;
    types.reserve(ranked.size());
    for (const auto& r : ranked) types.push_back(std::get<3>(r));
    return types;
}

void NodeGraphView::filterSearch(const QString& text) {
    searchList_->clear();
    searchAbout_->clear();
    if (!document_) return;
    // Maya's ranking; each type by its label, its menu category after it in the category's color.
    for (const std::string& type : nodeSearchResults(*document_, text)) {
        const nlohmann::json* spec = document_->spec(type);
        const std::string label = spec->value("label", std::string());
        const QString shown = QString::fromStdString(label.empty() ? nodeLabel(type) : label);
        auto* item = new QListWidgetItem(shown + QStringLiteral("    ") + QString::fromStdString(spec->value("categoryTitle", std::string())), searchList_);
        item->setForeground(theme::nodeCategoryColor(spec->value("category", std::string())).lighter(130));
        item->setData(Qt::UserRole, QString::fromStdString(type));
        item->setToolTip(QString::fromStdString(spec->value("description", std::string())));
    }
    if (searchList_->count() > 0) searchList_->setCurrentRow(0);
    else searchAbout_->setText(tr("No node matches. Try what it does: symmetry, density, frizz, collision..."));
}

void NodeGraphView::setSearchText(const QString& text) {
    if (!searchOpen()) openSearch();
    searchEdit_->setText(text);
}

QStringList NodeGraphView::searchRows() const {
    QStringList rows;
    for (int i = 0; i < searchList_->count(); ++i) rows << searchList_->item(i)->text();
    return rows;
}

std::vector<std::string> NodeGraphView::searchTypes() const {
    std::vector<std::string> types;
    for (int i = 0; i < searchList_->count(); ++i) types.push_back(searchList_->item(i)->data(Qt::UserRole).toString().toStdString());
    return types;
}

QString NodeGraphView::searchAbout() const { return searchAbout_->text(); }

void NodeGraphView::addFromSearch() {
    QListWidgetItem* item = searchList_->currentItem();
    if (!item || !document_) return;
    const std::string type = item->data(Qt::UserRole).toString().toStdString();
    closeSearch();
    // After the selected node when the new one takes what it gives: placed to its right and fed
    // by it; else at the cursor.
    int after = -1;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].name == selected_) after = static_cast<int>(i);
    }
    QPointF at = searchScene_;
    std::string feedInput;
    if (after >= 0 && !nodes_[after].outputs.empty()) {
        if (const nlohmann::json* spec = document_->spec(type)) {
            for (const nlohmann::json& p : spec->value("inputs", nlohmann::json::array())) {
                if (p.value("type", std::string()) == nodes_[after].outputs[0].type) {
                    feedInput = p.value("name", std::string());
                    break;
                }
            }
        }
        if (!feedInput.empty()) at = nodes_[after].pos + QPointF(kColumn, 0);
    }
    std::string name;
    std::string error = document_->addNode(type, static_cast<float>(at.x()), static_cast<float>(at.y()), &name);
    if (error.empty() && !feedInput.empty()) {
        error = document_->connectNodes(nodes_[after].name, {}, name, feedInput);
    }
    if (!error.empty()) {
        emit message(QString::fromStdString(error));
    }
    emit edited();
    refresh();
    selectNode(name);
}

bool NodeGraphView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == promptEdit_ && event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        prompt_->hide();
        promptDone_ = nullptr;
        setFocus();
        return true;
    }
    // Escape in the search closes it; Tab there stays in the search.
    if ((watched == searchEdit_ || watched == searchList_) && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape) {
            closeSearch();
            return true;
        }
        if (key->key() == Qt::Key_Down && watched == searchEdit_ && searchList_->count() > 0) {
            searchList_->setCurrentRow(std::min(searchList_->currentRow() + 1, searchList_->count() - 1));
            return true;
        }
        if (key->key() == Qt::Key_Up && watched == searchEdit_ && searchList_->count() > 0) {
            searchList_->setCurrentRow(std::max(searchList_->currentRow() - 1, 0));
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void NodeGraphView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (!fitted_ && !nodes_.empty()) fitAll();
}

void NodeGraphView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!fitted_ && !nodes_.empty()) fitAll();
}

}  // namespace groomlab::app

// SPDX-License-Identifier: Apache-2.0
#include "node_graph.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QKeyEvent>
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

namespace groomlab::app {

namespace {

constexpr qreal kNodeWidth = 160.0;
constexpr qreal kHeader = 28.0;
constexpr qreal kRow = 18.0;
constexpr qreal kBodyPadding = 8.0;
constexpr qreal kRadius = 14.0;  // playful.json's cornerRadius
constexpr qreal kPortRadius = 5.0;
constexpr qreal kColumn = 230.0;  // the auto layout's columns and rows
constexpr qreal kRowGap = 120.0;

// Dark words on a bright header (yellow, white, emerald), white on the others.
QColor headerText(const QColor& fill) {
    const qreal luminance = 0.299 * fill.redF() + 0.587 * fill.greenF() + 0.114 * fill.blueF();
    return luminance > 0.6 ? theme::ink() : Qt::white;
}

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
    layout->addWidget(searchEdit_);
    layout->addWidget(searchList_, 1);
    search_->resize(260, 320);
    search_->hide();
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
    promptTitle_->setStyleSheet(QStringLiteral("color: #F3F1EC;"));
    promptEdit_ = new QLineEdit(prompt_);
    promptEdit_->setFont(theme::uiFont(14, 600));
    auto* hint = new QLabel(tr("Enter saves, Esc cancels"), prompt_);
    hint->setFont(theme::uiFont(11, 600));
    hint->setStyleSheet(QStringLiteral("color: #8F8B84;"));
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
    for (const GroomDocument::NodeInfo& info : document_->nodes()) {
        Node node;
        node.name = info.name;
        node.type = info.type;
        node.bypass = info.bypass;
        node.evaluation = info.evaluation;
        if (const nlohmann::json* spec = document_->spec(info.type)) {
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
        wire.path = wirePath(outputPos(from, wire.output), inputPos(to, wire.input));
        wires_.push_back(wire);
    }
    if (selectedWire_ >= static_cast<int>(wires_.size())) selectedWire_ = -1;
    if (!selected_.empty() && index(selected_) < 0) {
        selected_.clear();
        emit selectionChanged(selected_);
    }
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

QPainterPath NodeGraphView::wirePath(const QPointF& from, const QPointF& to) const {
    QPainterPath path(from);
    const qreal dx = std::max(60.0, std::abs(to.x() - from.x()) * 0.5);
    path.cubicTo(from + QPointF(dx, 0), to - QPointF(dx, 0), to);
    return path;
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

void NodeGraphView::setFitMargins(const QMargins& margins) {
    fitMargins_ = margins;
    if (fitted_ && isVisible()) fitAll();
}

void NodeGraphView::selectNode(const std::string& name) {
    if (name == selected_) return;
    selected_ = name;
    selectedWire_ = -1;
    update();
    emit selectionChanged(selected_);
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
    const QColor color = theme::nodeCategoryColor(node.category);
    painter.setOpacity(node.bypass ? 0.5 : 1.0);
    // The body, then the header clipped to the top corners.
    QPainterPath body;
    body.addRoundedRect(rect, kRadius, kRadius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::nodeBody());
    painter.drawPath(body);
    painter.save();
    painter.setClipPath(body);
    painter.setBrush(color);
    painter.drawRect(QRectF(rect.topLeft(), QSizeF(rect.width(), kHeader)));
    painter.restore();
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
    // A note (the node does nothing as set): a grey "i" on the node's corner.
    if (notes_.count(node.name)) {
        const QPointF corner(rect.right(), rect.top());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(128, 132, 140));
        painter.drawEllipse(corner, 8.0, 8.0);
        painter.setFont(theme::uiFont(10, 800));
        painter.setPen(QColor(245, 245, 245));
        painter.drawText(QRectF(corner.x() - 8.0, corner.y() - 8.0, 16.0, 16.0), Qt::AlignCenter, QStringLiteral("i"));
    }
    // When it runs, unless always: a tag under the card.
    if (node.evaluation == "viewportOnly" || node.evaluation == "renderOnly") {
        painter.setFont(theme::uiFont(10, 600));
        painter.setPen(QColor(142, 150, 158));
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
        painter.drawText(QRectF(rect.x(), rect.bottom() - 16, rect.width(), 14), Qt::AlignCenter, QString::fromStdString(node.type));
    }
    painter.setOpacity(1.0);
    if (selected) {
        painter.setPen(QPen(theme::white(), 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect.adjusted(-1, -1, 1, 1), kRadius + 1, kRadius + 1);
    }
}

void NodeGraphView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.translate(pan_);
    painter.scale(scale_, scale_);
    // The backdrops first: a tinted frame with its title, the groom still showing through.
    for (const Backdrop& b : backdrops_) {
        QColor fill = b.color;
        fill.setAlpha(46);
        painter.setPen(QPen(QColor(b.color.red(), b.color.green(), b.color.blue(), 120), 1.5));
        painter.setBrush(fill);
        painter.drawRoundedRect(b.rect, kRadius, kRadius);
        painter.setFont(theme::uiFont(14, 800));
        painter.setPen(theme::textSoft());
        painter.drawText(b.rect.adjusted(14, 8, -14, -8), Qt::AlignTop | Qt::AlignLeft, b.title);
    }
    // The wires, the one being dragged with them, then the nodes, the selected one last.
    for (std::size_t i = 0; i < wires_.size(); ++i) {
        const Wire& w = wires_[i];
        if (dragWireInputNode_ == w.to && dragWireInputPort_ == w.input && drag_ == Drag::Wire) continue;  // picked up
        const bool selected = static_cast<int>(i) == selectedWire_;
        QColor color = theme::portColor(nodes_[w.from].outputs[w.output].type);
        color.setAlpha(selected ? 255 : 220);
        painter.setPen(QPen(selected ? theme::white() : color, selected ? 3.0 : 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(w.path);
    }
    if (drag_ == Drag::Wire && dragWireFrom_ >= 0) {
        QColor color = theme::portColor(nodes_[dragWireFrom_].outputs[dragWirePort_].type);
        painter.setPen(QPen(color, 2.0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(wirePath(outputPos(nodes_[dragWireFrom_], dragWirePort_), dragWireTo_));
    }
    int selectedIndex = -1;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].name == selected_) {
            selectedIndex = static_cast<int>(i);
            continue;
        }
        drawNode(painter, nodes_[i], false);
    }
    if (selectedIndex >= 0) drawNode(painter, nodes_[selectedIndex], true);
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
        drag_ = Drag::Move;
        dragNode_ = node;
        selectNode(nodes_[node].name);
        return;
    }
    const int wire = wireAt(pressScene_);
    if (wire >= 0) {
        selectedWire_ = wire;
        if (!selected_.empty()) {
            selected_.clear();
            emit selectionChanged(selected_);
        }
        update();
        return;
    }
    drag_ = Drag::Pan;
    selectedWire_ = -1;
    selectNode({});
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
                nodes_[dragNode_].pos += delta / scale_;
                // The wires follow the card.
                for (Wire& w : wires_) {
                    if (w.from == dragNode_ || w.to == dragNode_) w.path = wirePath(outputPos(nodes_[w.from], w.output), inputPos(nodes_[w.to], w.input));
                }
                update();
            }
            break;
        case Drag::Wire:
            dragWireTo_ = toScene(now);
            update();
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
    if (drag == Drag::Move && dragNode_ >= 0 && document_) {
        const Node& n = nodes_[dragNode_];
        if ((n.pos - pressScene_).manhattanLength() > 0.0) {
            document_->setNodePosition(n.name, static_cast<float>(n.pos.x()), static_cast<float>(n.pos.y()));
            laidOut_.erase(n.name);
        }
        dragNode_ = -1;
        return;
    }
    if (drag == Drag::Wire && document_) {
        const QPointF at = toScene(event->position());
        int node = -1, port = -1;
        bool output = false;
        std::string error;
        if (portAt(at, &node, &port, &output) && !output && node != dragWireFrom_) {
            const Node& from = nodes_[dragWireFrom_];
            const Node& to = nodes_[node];
            const bool first = dragWirePort_ == 0;
            if (dragWireInputNode_ == node && dragWireInputPort_ == port) {
                // Dropped back where it was.
            } else {
                error = document_->connectNodes(from.name, first ? std::string() : from.outputs[dragWirePort_].name, to.name,
                                                to.inputs[port].name);
                if (error.empty()) emit edited();
            }
        } else if (dragWireInputNode_ >= 0) {
            error = document_->disconnectInput(nodes_[dragWireInputNode_].name, nodes_[dragWireInputNode_].inputs[dragWireInputPort_].name);
            if (error.empty()) emit edited();
        }
        if (!error.empty()) emit message(QString::fromStdString(error));
        dragWireFrom_ = dragWirePort_ = dragWireInputNode_ = dragWireInputPort_ = -1;
        refresh();
    }
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
        emit edited();
        refresh();
        emit selectionChanged(selected_);
    });
    setPromptText(QString::fromStdString(name));
}

void NodeGraphView::duplicateSelected() {
    if (!document_ || selected_.empty()) return;
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

void NodeGraphView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || event->modifiers().testFlag(Qt::AltModifier)) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    const QPointF scene = toScene(event->position());
    const int node = nodeAt(scene);
    if (node >= 0 && scene.y() - nodes_[node].pos.y() <= kHeader) {
        drag_ = Drag::None;
        selectNode(nodes_[node].name);
        renameSelected();
    }
    event->accept();
}

void NodeGraphView::contextMenuEvent(QContextMenuEvent* event) {
    if (!document_) return;
    const QPointF scene = toScene(QPointF(event->pos()));
    const int node = nodeAt(scene);
    QMenu menu(this);
    if (node >= 0) {
        selectNode(nodes_[node].name);
        const bool output = nodes_[node].type == "Output";
        menu.addAction(tr("Rename..."), this, [this] { renameSelected(); });
        menu.addAction(tr("Duplicate"), this, [this] { duplicateSelected(); })->setEnabled(!output);
        menu.addAction(tr("Set as Output"), this, [this] { setSelectedAsOutput(); });
        menu.addAction(nodes_[node].bypass ? tr("Enable") : tr("Bypass"), this, [this, name = nodes_[node].name, on = !nodes_[node].bypass] {
            setBypass(name, on);
        });
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
        menu.addAction(tr("Delete"), this, [this] {
            if (!document_ || selected_.empty()) return;
            const std::string error = document_->removeNode(selected_);
            if (!error.empty()) emit message(QString::fromStdString(error));
            else emit edited();
            refresh();
        })->setEnabled(!output);
    } else {
        menu.addAction(tr("Add node..."), this, [this] { openSearch(); });
        menu.addAction(tr("Tidy layout"), this, [this] { tidyLayout(); });
        menu.addAction(tr("Fit"), this, [this] { fitAll(); });
    }
    menu.exec(event->globalPos());
    event->accept();
}

void NodeGraphView::keyPressEvent(QKeyEvent* event) {
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
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
            fitAll();
            event->accept();
            return;
        case Qt::Key_B:
            for (const Node& n : nodes_) {
                if (n.name == selected_) setBypass(n.name, !n.bypass);
            }
            event->accept();
            return;
        case Qt::Key_Delete:
        case Qt::Key_Backspace: {
            if (!document_) break;
            std::string error;
            if (selectedWire_ >= 0 && selectedWire_ < static_cast<int>(wires_.size())) {
                const Wire& w = wires_[selectedWire_];
                error = document_->disconnectInput(nodes_[w.to].name, nodes_[w.to].inputs[w.input].name);
                selectedWire_ = -1;
            } else if (!selected_.empty()) {
                error = document_->removeNode(selected_);
            } else {
                break;
            }
            if (!error.empty()) emit message(QString::fromStdString(error));
            else emit edited();
            refresh();
            event->accept();
            return;
        }
        default:
            break;
    }
    QWidget::keyPressEvent(event);
}

bool NodeGraphView::searchOpen() const { return !search_->isHidden(); }

void NodeGraphView::openSearch() {
    const QPointF at = mapFromGlobal(QCursor::pos());
    const QPointF where = rect().contains(at.toPoint()) ? at : QPointF(width() / 2.0, height() / 2.0);
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

void NodeGraphView::filterSearch(const QString& text) {
    searchList_->clear();
    if (!document_) return;
    const QString needle = text.trimmed().toLower();
    for (const std::string& type : document_->operatorTypes()) {
        const QString name = QString::fromStdString(type);
        if (!needle.isEmpty() && !name.toLower().contains(needle)) continue;
        auto* item = new QListWidgetItem(name, searchList_);
        const nlohmann::json* spec = document_->spec(type);
        const std::string category = spec ? spec->value("category", std::string()) : std::string();
        item->setForeground(theme::nodeCategoryColor(category).lighter(130));
        item->setData(Qt::UserRole, name);
        item->setToolTip(spec ? QString::fromStdString(spec->value("description", std::string())) : QString());
    }
    if (searchList_->count() > 0) searchList_->setCurrentRow(0);
}

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

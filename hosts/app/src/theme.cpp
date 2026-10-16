// SPDX-License-Identifier: Apache-2.0
#include "theme.h"

#include <algorithm>

#include <QFile>
#include <QFontDatabase>

#include <nlohmann/json.hpp>

namespace groomlab::app::theme {

// ---- Colors --------------------------------------------------------------------------------------

namespace {

// hosts/maya/themes/playful.json, the Maya host's theme, embedded at build time (a resource of
// this library): the palette, the UI tones, the graph's and the icons' colors come from that one
// file, so a theme edit reaches both hosts.
const nlohmann::json& playful() {
    static const nlohmann::json theme = [] {
        QFile file(QStringLiteral(":/groomlab/themes/playful.json"));
        nlohmann::json parsed = file.open(QIODevice::ReadOnly)
                                    ? nlohmann::json::parse(file.readAll().toStdString(), nullptr, false)
                                    : nlohmann::json();
        return parsed.is_object() ? parsed : nlohmann::json::object();
    }();
    return theme;
}

// A color as the theme writes one: "#RRGGBB", a palette name ("blue"), or a ui key naming one
// ("accent"); an invalid color for anything else.
QColor resolve(const std::string& value, int depth = 0) {
    if (!value.empty() && value[0] == '#') return QColor(QString::fromStdString(value));
    const nlohmann::json& t = playful();
    for (const char* section : {"palette", "ui"}) {
        const auto s = t.find(section);
        if (s == t.end() || !s->is_object()) continue;
        const auto v = s->find(value);
        if (v != s->end() && v->is_string() && depth < 4) return resolve(v->get<std::string>(), depth + 1);
    }
    return QColor();
}

// The color at section.key (graph.categories.Look: {"graph", "categories"}, "Look").
QColor themed(std::initializer_list<const char*> path, const std::string& key) {
    const nlohmann::json* at = &playful();
    for (const char* p : path) {
        const auto next = at->find(p);
        if (next == at->end() || !next->is_object()) return QColor();
        at = &*next;
    }
    const auto v = at->find(key);
    return v != at->end() && v->is_string() ? resolve(v->get<std::string>()) : QColor();
}

}  // namespace

// The palette from playful.json, then the mockup's glass and text tones (the app's own).
QColor red() { return themed({"palette"}, "red"); }
QColor yellow() { return themed({"palette"}, "yellow"); }
QColor blue() { return themed({"palette"}, "blue"); }
QColor emerald() { return themed({"palette"}, "emerald"); }
QColor purple() { return themed({"palette"}, "purple"); }
QColor white() { return themed({"palette"}, "white"); }
QColor uiColor(const std::string& key) { return themed({"ui"}, key); }
QColor glass() { return QColor(31, 32, 37, 184); }
QColor glassSolid() { return QColor(31, 32, 37, 219); }
QColor text() { return QColor(0xF3, 0xF1, 0xEC); }
QColor textSoft() { return QColor(0xD9, 0xD5, 0xCD); }
QColor textDim() { return QColor(0xB8, 0xB4, 0xAC); }
QColor textMuted() { return QColor(0x8F, 0x8B, 0x84); }
QColor track() { return QColor(255, 255, 255, 31); }
QColor chip() { return QColor(255, 255, 255, 20); }
QColor divider() { return QColor(255, 255, 255, 31); }
QColor renderRed() { return QColor(0xE5, 0x38, 0x3B); }
QColor backdrop() { return QColor(0x17, 0x18, 0x1B); }
QColor ink() { return QColor(0x1B, 0x1A, 0x17); }
QColor blueLight() { return QColor(0x6E, 0xA4, 0xFF); }
QColor redLight() { return QColor(0xFF, 0x7A, 0x7C); }
QColor emeraldSoft() { return QColor(0x7E, 0xE2, 0xB8); }
QColor emeraldChip() { return QColor(46, 204, 143, 56); }

// ---- Fonts ---------------------------------------------------------------------------------------

namespace {

// The bundled variable fonts (google/fonts, SIL OFL 1.1: THIRD_PARTY_NOTICES.txt). Qt
// registers a variable font's named instances, so a weight picks the right one.
bool fontsLoaded = false, fontsFound = false;

}  // namespace

bool loadFonts() {
    if (fontsLoaded) return fontsFound;
    fontsLoaded = true;
    const int figtree = QFontDatabase::addApplicationFont(QStringLiteral(":/groomlab/fonts/Figtree[wght].ttf"));
    const int mono = QFontDatabase::addApplicationFont(QStringLiteral(":/groomlab/fonts/JetBrainsMono[wght].ttf"));
    fontsFound = figtree >= 0 && mono >= 0;
    return fontsFound;
}

QString uiFamily() { return QStringLiteral("Figtree"); }

QString monoFamily() { return QStringLiteral("JetBrains Mono"); }

QFont uiFont(int pixelSize, int weight) {
    loadFonts();
    QFont f(uiFamily());
    f.setStyleHint(QFont::SansSerif);
    f.setPixelSize(pixelSize);
    f.setWeight(static_cast<QFont::Weight>(weight));
    return f;
}

QFont monoFont(int pixelSize, int weight) {
    loadFonts();
    QFont f(monoFamily());
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(pixelSize);
    f.setWeight(static_cast<QFont::Weight>(weight));
    return f;
}

// ---- Brushes --------------------------------------------------------------------------------------

const std::vector<BrushEntry>& brushes() {
    // The core's catalog in its order (Create, Shape, Trim, Paint, then Freeze), each tip its
    // label and hint.
    static const std::vector<BrushEntry> entries = [] {
        std::vector<BrushEntry> out;
        for (const groomlab::BrushEntry& e : brushCatalog()) {
            if (e.id == "select") continue;
            out.push_back({e.id, e.icon, e.label + ": " + e.hint, e.category});
        }
        return out;
    }();
    return entries;
}

const BrushEntry* brush(const std::string& name) {
    for (const BrushEntry& b : brushes()) {
        if (name == b.name) return &b;
    }
    return nullptr;
}

QColor categoryColor(BrushCategory category) {
    // playful.json's icons (create: the tools that make guides); the guide tools white, as Select is.
    switch (category) {
        case BrushCategory::Create: return themed({"icons"}, "create");
        case BrushCategory::Shape: return themed({"icons"}, "shape");
        case BrushCategory::Trim: return themed({"icons"}, "trim");
        case BrushCategory::Paint: return themed({"icons"}, "paint");
        case BrushCategory::Guides: return white();
    }
    return blue();
}

QColor categoryIconColor(BrushCategory category) {
    // Lighter than the fills, as the mockup tints the rail's resting icons, so they read on glass.
    switch (category) {
        case BrushCategory::Create: return emerald();
        case BrushCategory::Shape: return blueLight();
        case BrushCategory::Trim: return redLight();
        case BrushCategory::Paint: return yellow();
        case BrushCategory::Guides: return text();
    }
    return blueLight();
}

QString brushTitle(const std::string& name) {
    if (name.empty()) return {};
    if (const groomlab::BrushEntry* e = brushEntry(name)) return QString::fromStdString(e->label);
    QString title = QString::fromStdString(name);
    title[0] = title[0].toUpper();
    return title;
}

// ---- The node graph ------------------------------------------------------------------------------

QColor nodeCategoryColor(const std::string& category) {
    // playful.json's graph.categories (the Graph Editor's titles), by the operators' lowercase
    // category names.
    const char* title = category == "source"     ? "Sources"
                        : category == "grow"     ? "Grow"
                        : category == "shape"    ? "Shape"
                        : category == "color"    ? "Color"
                        : category == "look"     ? "Look"
                        : category == "field"    ? "Fields & Curves"
                        : category == "motion"   ? "Motion"
                        : category == "feathers" ? "Feathers"
                        : category == "output"   ? "Output"
                                                 : "Utility";
    return themed({"graph", "categories"}, title);
}

QColor portColor(const std::string& type) {
    // playful.json's graph.ports, by the port types' names (Strands, Guides, ...: lowercase here).
    const char* name = type == "strands" ? "Strands"
                       : type == "guides" ? "Guides"
                       : type == "mesh"   ? "Mesh"
                       : type == "field"  ? "Field"
                       : type == "curve"  ? "Curve"
                                          : "Curves";  // and anything else
    return themed({"graph", "ports"}, name);
}

QColor nodeBody() { return themed({"graph"}, "node"); }

// ---- Layout --------------------------------------------------------------------------------------

OverlayLayout layoutOverlay(const OverlayRequest& r, const OverlayMetrics& m) {
    OverlayLayout out;
    const int w = r.view.width(), h = r.view.height();
    // The top row: everything centered on one line 40 px down, as the mockup has it.
    const int topCenter = 40;
    out.logo = QRect(m.margin + 4, topCenter - r.logo.height() / 2, r.logo.width(), r.logo.height());
    const int modesHeight = 48;
    out.modes = QRect((w - r.modesWidth) / 2, topCenter - modesHeight / 2, r.modesWidth, modesHeight);
    out.topRight = QRect(w - m.margin - 4 - r.topRightWidth, topCenter - m.topHeight / 2, r.topRightWidth, m.topHeight);

    out.bar = QRect((w - r.barWidth) / 2, h - m.barBottom - m.barHeight, r.barWidth, m.barHeight);
    out.lookCard = QRect(w - m.margin - m.lookCardWidth, m.lookCardTop, m.lookCardWidth, r.lookCardHeight);
    // The Graph and Presets pills sit at the bottom left in every mode (the close button after
    // them while the graph shows); the stats above them.
    out.graphPill = QRect(m.margin + 4, h - 28 - r.graphPill.height(), r.graphPill.width(), r.graphPill.height());
    out.graphTools = QRect(out.graphPill.right() + 1 + 8, out.graphPill.top(), r.graphToolsWidth, r.graphPill.height());
    const int statsBottom = out.graphPill.top() - 12;
    out.stats = QRect(m.margin + 4, statsBottom - r.stats.height(), r.stats.width(), r.stats.height());

    // The brush rail between the top row and the stats line, its buttons shrinking to fit;
    // centered in that span. When even the smallest buttons do not fit (a short window), the
    // rail takes the window's whole height instead and the stats and the pills move to its
    // right, so nothing is drawn over anything.
    const int clearance = 72;
    int railTop = clearance, railBottom = std::min(h - clearance, out.stats.top() - 12);
    int available = std::max(0, railBottom - railTop);
    int gap = m.railGap, divider = m.railDivider;
    auto fixedHeight = [&] { return 2 * m.railPadding + r.railDividers * divider + std::max(0, r.railButtons - 1) * gap; };
    int button = m.railButton;
    auto fit = [&] { button = std::clamp((available - fixedHeight()) / std::max(1, r.railButtons), m.railButtonMin, m.railButton); };
    auto fits = [&] { return r.railButtons == 0 || fixedHeight() + r.railButtons * button <= available; };
    if (!fits()) fit();
    if (!fits()) {
        // Squeezed: the gaps and dividers tightened first ...
        gap = 2;
        divider = 5;
        fit();
    }
    if (!fits()) {
        // ... then the full height, the stats and the pills beside the rail, the buttons as
        // small as they must be (below the usual minimum only in a very short window).
        railTop = clearance - 8;
        railBottom = h - m.margin;
        available = std::max(0, railBottom - railTop);
        fit();
        if (!fits()) {
            // The tightest rows before the buttons shrink further.
            gap = 1;
            divider = 3;
            fit();
        }
        if (!fits()) button = std::clamp((available - fixedHeight()) / std::max(1, r.railButtons), 22, m.railButton);
        const int beside = m.margin + std::min(m.railWidth, button + 12) + 16;
        out.graphPill.moveLeft(beside);
        out.graphTools.moveLeft(out.graphPill.right() + 1 + 8);
        out.stats.moveLeft(beside);
    }
    out.railButton = button;
    out.railGap = gap;
    out.railDivider = divider;
    const int railHeight = fixedHeight() + r.railButtons * button;
    const int railWidth = std::min(m.railWidth, button + 12);
    out.rail = QRect(m.margin, railTop + std::max(0, available - railHeight) / 2, railWidth, railHeight);

    // The brush bar keeps clear of the pills: moved right while there is room, else above them.
    const int pillsRight = out.graphTools.right() + 1;
    if (r.barShown && out.bar.left() < pillsRight + 12) {
        if (pillsRight + 12 + r.barWidth <= w - m.margin) out.bar.moveLeft(pillsRight + 12);
        else out.bar.moveBottom(std::min(out.graphPill.top(), out.stats.top()) - 12 - 1);
    }

    const int panelHeight = r.panelButtons * m.panelButton + std::max(0, r.panelButtons - 1) * m.panelGap;
    out.panelRail = QRect(w - m.margin - m.panelButton, (h - panelHeight) / 2, m.panelButton, panelHeight);

    out.refine = QRect(w - m.margin - 4 - r.refine.width(), h - m.statsBottom - r.refine.height(), r.refine.width(), r.refine.height());
    return out;
}

// ---- The style sheet ------------------------------------------------------------------------------

QString styleSheet() {
    // The ordinary widgets inside the glass panels (the full brush, look, guides and parameter
    // panels): no frames, round corners, the raised tone for fields, the accent for what is on.
    QString sheet = QStringLiteral(R"(
QWidget { color: #F3F1EC; font-family: "Figtree"; font-size: 13px; background: transparent; }
QToolTip { color: #F3F1EC; background: @panel@; border: 1px solid @hover@; border-radius: 6px; padding: 4px 8px; }
QMenu { background: @panel@; border: 1px solid @hover@; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 6px 28px 6px 12px; border-radius: 6px; }
QMenu::item:selected { background: @accent@; color: @accentText@; }
QMenu::item:disabled { color: #8F8B84; }
QMenu::separator { height: 1px; background: @hover@; margin: 4px 8px; }
QGroupBox { border: 1px solid rgba(255,255,255,0.10); border-radius: 12px; margin-top: 14px; padding: 8px 4px 4px 4px; font-weight: 700; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #B8B4AC; }
QLabel { color: #D9D5CD; }
QAbstractSpinBox, QLineEdit, QComboBox {
    background: rgba(255,255,255,0.08); border: 1px solid transparent; border-radius: 8px; padding: 3px 6px; min-height: 20px;
    selection-background-color: @accent@; }
QAbstractSpinBox:focus, QLineEdit:focus, QComboBox:focus { border: 1px solid @accent@; }
QAbstractSpinBox:disabled, QLineEdit:disabled, QComboBox:disabled { color: #8F8B84; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { width: 14px; border: none; background: transparent; }
QAbstractSpinBox::up-arrow { image: none; width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent; border-bottom: 5px solid #B8B4AC; }
QAbstractSpinBox::down-arrow { image: none; width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid #B8B4AC; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox::down-arrow { image: none; width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid #B8B4AC; margin-right: 6px; }
QComboBox QAbstractItemView { background: @panel@; border: 1px solid @hover@; border-radius: 8px; padding: 4px; selection-background-color: @accent@; outline: none; }
QCheckBox { spacing: 8px; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 5px; background: rgba(255,255,255,0.10); border: 1px solid rgba(255,255,255,0.14); }
QCheckBox::indicator:checked { background: @accent@; border-color: @accent@; }
QCheckBox::indicator:disabled { background: rgba(255,255,255,0.05); }
QPushButton { background: rgba(255,255,255,0.08); border: none; border-radius: 10px; padding: 6px 12px; min-height: 18px; font-weight: 600; }
QPushButton:hover { background: rgba(255,255,255,0.14); }
QPushButton:pressed { background: @accent@; color: @accentText@; }
QPushButton:disabled { color: #8F8B84; background: rgba(255,255,255,0.04); }
QSlider::groove:horizontal { height: 8px; border-radius: 4px; background: rgba(255,255,255,0.12); }
QSlider::sub-page:horizontal { background: @accent@; border-radius: 4px; }
QSlider::handle:horizontal { width: 16px; height: 16px; margin: -4px 0; border-radius: 8px; background: #F3F1EC; }
QScrollArea { border: none; }
QScrollBar:vertical { width: 8px; background: transparent; margin: 0; }
QScrollBar::handle:vertical { background: rgba(255,255,255,0.18); border-radius: 4px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: rgba(255,255,255,0.30); }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { height: 8px; background: transparent; margin: 0; }
QScrollBar::handle:horizontal { background: rgba(255,255,255,0.18); border-radius: 4px; min-width: 24px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QTreeWidget, QListWidget { background: rgba(255,255,255,0.05); border: none; border-radius: 10px; padding: 4px; outline: none; alternate-background-color: transparent; }
QTreeWidget::item, QListWidget::item { padding: 3px; border-radius: 6px; }
QTreeWidget::item:selected, QListWidget::item:selected { background: @accent@; color: @accentText@; }
QTreeWidget::item:hover, QListWidget::item:hover { background: rgba(255,255,255,0.08); }
QHeaderView::section { background: transparent; color: #8F8B84; border: none; padding: 2px 4px; font-weight: 600; }
QTreeWidget::indicator { width: 14px; height: 14px; border-radius: 4px; background: rgba(255,255,255,0.10); }
QTreeWidget::indicator:checked { background: @emerald@; }
QTreeView::branch { background: transparent; }
)");
    // The theme's tones (playful.json's ui and palette) into the sheet.
    return sheet.replace(QStringLiteral("@accent@"), uiColor("accent").name())
        .replace(QStringLiteral("@accentText@"), uiColor("accentText").name())
        .replace(QStringLiteral("@panel@"), uiColor("panel").name())
        .replace(QStringLiteral("@hover@"), uiColor("hover").name())
        .replace(QStringLiteral("@emerald@"), emerald().name());
}

QSize fitToAvailable(QSize wanted, const QRect& available, double share, QSize minimum) {
    if (available.isEmpty()) return wanted;
    const int maxW = std::max(1, static_cast<int>(available.width() * share));
    const int maxH = std::max(1, static_cast<int>(available.height() * share));
    const int w = std::min(wanted.width(), maxW), h = std::min(wanted.height(), maxH);
    return {std::min(std::max(w, minimum.width()), available.width()), std::min(std::max(h, minimum.height()), available.height())};
}

}  // namespace groomlab::app::theme

// SPDX-License-Identifier: Apache-2.0
#include "theme.h"

#include <algorithm>
#include <cctype>
#include <type_traits>

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QSettings>

#include <nlohmann/json.hpp>

namespace groomlab::app::theme {

// ---- Colors --------------------------------------------------------------------------------------

namespace {

const QString kDefaultTheme = QStringLiteral("playful");
const QString kThemeSetting = QStringLiteral("ui/theme");

// hosts/maya/themes/<name>.json, the Maya host's theme files, embedded at build time (resources of
// this library): the palette, the UI tones, the graph's and the icons' colors come from those
// files, so a theme edit reaches both hosts.
nlohmann::json readTheme(const QString& name) {
    QFile file(QStringLiteral(":/groomlab/themes/%1.json").arg(name));
    nlohmann::json parsed = file.open(QIODevice::ReadOnly) ? nlohmann::json::parse(file.readAll().toStdString(), nullptr, false)
                                                           : nlohmann::json();
    return parsed.is_object() ? parsed : nlohmann::json::object();
}

void merge(nlohmann::json& base, const nlohmann::json& over) {
    for (auto it = over.begin(); it != over.end(); ++it) {
        if (it->is_object() && base.contains(it.key()) && base[it.key()].is_object()) merge(base[it.key()], *it);
        else base[it.key()] = *it;
    }
}

// A theme filled in from the one it extends (Playful unless it says otherwise), as the Maya
// host's theme.load does.
nlohmann::json loadTheme(const QString& name, int depth = 0) {
    nlohmann::json data = readTheme(name);
    const QString base = data.contains("extends") && data["extends"].is_string()
                             ? QString::fromStdString(data["extends"].get<std::string>())
                             : (name == kDefaultTheme ? QString() : kDefaultTheme);
    if (base.isEmpty() || depth > 4) return data;
    nlohmann::json out = loadTheme(base, depth + 1);
    merge(out, data);
    return out;
}

QString gTheme = kDefaultTheme;

const nlohmann::json& current() {
    static nlohmann::json data;
    static QString loaded;
    if (loaded != gTheme || data.is_null()) {
        data = loadTheme(gTheme);
        loaded = gTheme;
    }
    return data;
}

// A color as the theme writes one: "#RRGGBB", a palette name ("blue"), or a ui key naming one
// ("accent"); an invalid color for anything else.
QColor resolve(const std::string& value, int depth = 0) {
    if (!value.empty() && value[0] == '#') return QColor(QString::fromStdString(value));
    const nlohmann::json& t = current();
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
    const nlohmann::json* at = &current();
    for (const char* p : path) {
        const auto next = at->find(p);
        if (next == at->end() || !next->is_object()) return QColor();
        at = &*next;
    }
    const auto v = at->find(key);
    return v != at->end() && v->is_string() ? resolve(v->get<std::string>()) : QColor();
}

// The value at section.key, or `fallback` when the theme has none of that type.
template <typename T>
T value(const char* section, const char* key, T fallback) {
    const nlohmann::json& t = current();
    const auto s = t.find(section);
    if (s == t.end() || !s->is_object()) return fallback;
    const auto v = s->find(key);
    if (v == s->end()) return fallback;
    if constexpr (std::is_same_v<T, bool>) return v->is_boolean() ? v->get<bool>() : fallback;
    else return v->is_number() ? v->get<T>() : fallback;
}

// a toward b by t (0: a, 1: b).
QColor mix(const QColor& a, const QColor& b, double t) {
    return QColor::fromRgbF(static_cast<float>(a.redF() + (b.redF() - a.redF()) * t), static_cast<float>(a.greenF() + (b.greenF() - a.greenF()) * t),
                            static_cast<float>(a.blueF() + (b.blueF() - a.blueF()) * t));
}

}  // namespace

// ---- The theme ------------------------------------------------------------------------------------

QStringList themeNames() {
    QStringList names;
    for (const QString& file : QDir(QStringLiteral(":/groomlab/themes")).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
        names << file.chopped(5);
    }
    return names;
}

QString themeLabel(const QString& name) {
    QStringList words = QString(name).replace(QLatin1Char('_'), QLatin1Char(' ')).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString& w : words) w[0] = w[0].toUpper();
    return words.join(QLatin1Char(' '));
}

QString currentTheme() { return gTheme; }

bool setTheme(const QString& name) {
    if (!themeNames().contains(name)) return false;
    gTheme = name;
    return true;
}

QString savedTheme() {
    const QString name = QSettings().value(kThemeSetting, kDefaultTheme).toString();
    return themeNames().contains(name) ? name : kDefaultTheme;
}

void saveTheme(const QString& name) { QSettings().setValue(kThemeSetting, name); }

bool glassy() { return value("ui", "glass", true); }

// The palette from the theme's file, then the glass and text tones: the mockup's own on glass,
// the theme's ui tones on a solid theme.
QColor red() { return themed({"palette"}, "red"); }
QColor yellow() { return themed({"palette"}, "yellow"); }
QColor blue() { return themed({"palette"}, "blue"); }
QColor emerald() { return themed({"palette"}, "emerald"); }
QColor purple() { return themed({"palette"}, "purple"); }
QColor white() { return themed({"palette"}, "white"); }
QColor uiColor(const std::string& key) { return themed({"ui"}, key); }
QColor glass() { return glassy() ? QColor(31, 32, 37, 184) : uiColor("panel"); }
QColor glassSolid() { return glassy() ? QColor(31, 32, 37, 219) : uiColor("panel"); }
QColor glassPanel() { return glassy() ? QColor(31, 32, 37, 236) : uiColor("panel"); }
QColor glassEdge() {
    if (glassy()) return QColor();
    const QColor outline = uiColor("outline");
    return outline.isValid() ? outline : uiColor("border");
}
QColor text() { return glassy() ? QColor(0xF3, 0xF1, 0xEC) : uiColor("text"); }
QColor textSoft() { return glassy() ? QColor(0xD9, 0xD5, 0xCD) : mix(uiColor("text"), uiColor("textDim"), 0.33); }
QColor textDim() { return glassy() ? QColor(0xB8, 0xB4, 0xAC) : mix(uiColor("text"), uiColor("textDim"), 0.66); }
QColor textMuted() { return glassy() ? QColor(0x8F, 0x8B, 0x84) : uiColor("textDim"); }
QColor track() { return QColor(255, 255, 255, 31); }
QColor chip() { return QColor(255, 255, 255, 20); }
QColor divider() { return QColor(255, 255, 255, 31); }
QColor renderRed() { return glassy() ? QColor(0xE5, 0x38, 0x3B) : uiColor("accent"); }
QColor backdrop() { return glassy() ? QColor(0x17, 0x18, 0x1B) : uiColor("background"); }
QColor ink() { return QColor(0x1B, 0x1A, 0x17); }
QColor blueLight() { return glassy() ? QColor(0x6E, 0xA4, 0xFF) : blue().lighter(115); }
QColor redLight() { return glassy() ? QColor(0xFF, 0x7A, 0x7C) : red().lighter(115); }
QColor emeraldSoft() { return glassy() ? QColor(0x7E, 0xE2, 0xB8) : emerald().lighter(115); }
QColor emeraldChip() {
    QColor c = emerald();
    c.setAlpha(56);
    return c;
}
QColor readableOn(const QColor& fill) {
    const qreal luminance = 0.299 * fill.redF() + 0.587 * fill.greenF() + 0.114 * fill.blueF();
    return luminance > 0.6 ? ink() : QColor(Qt::white);
}
QColor segmentFill(const QColor& own) { return glassy() ? own : uiColor("accent"); }
QColor segmentText(const QColor& own) { return glassy() ? own : uiColor("accentText"); }
QColor sliderFill(const QColor& own) {
    const auto ui = current().find("ui");
    if (ui == current().end() || !ui->is_object()) return own;
    const auto sliders = ui->find("sliders");
    if (sliders == ui->end() || !sliders->is_array() || sliders->size() != 1 || !(*sliders)[0].is_string()) return own;
    const QColor one = resolve((*sliders)[0].get<std::string>());
    return one.isValid() ? one : own;
}
QColor actionFill(const QColor& own) { return segmentFill(own); }
QColor actionText(const QColor& own) { return segmentText(own); }

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
    // The theme's icons (create: the tools that make guides); the guide tools white, as Select is.
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
    // Lighter than the fills, as the mockup tints the rail's resting icons, so they read on glass;
    // a solid theme's icons as its file colors them.
    if (!glassy()) return category == BrushCategory::Guides ? text() : categoryColor(category);
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
    // The theme's graph.categories (the Graph Editor's titles), by the operators' lowercase
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

QColor portColor(const std::string& portType) {
    // The theme's graph.ports, by the port types' names (Strands, Guides, ...: either case here).
    std::string type = portType;
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const char* name = type == "strands" ? "Strands"
                       : type == "guides" ? "Guides"
                       : type == "mesh"   ? "Mesh"
                       : type == "field"  ? "Field"
                       : type == "curve"  ? "Curve"
                       : type == "value"  ? "Value"
                                          : "Curves";  // and anything else
    return themed({"graph", "ports"}, name);
}

QColor nodeBody() { return themed({"graph"}, "node"); }

QColor nodeBorder() { return themed({"graph"}, "nodeBorder"); }

QColor nodeSelected() {
    const QColor c = themed({"graph"}, "selected");
    return c.isValid() ? c : white();
}

qreal nodeRadius() { return value("graph", "cornerRadius", 14.0); }

bool tintedHeaders() {
    const nlohmann::json& graph = current().value("graph", nlohmann::json::object());
    return graph.is_object() && graph.value("headerStyle", std::string("filled")) == "tinted";
}

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
QWidget { color: @text@; font-family: "Figtree"; font-size: 13px; background: transparent; }
QToolTip { color: @text@; background: @panel@; border: 1px solid @hover@; border-radius: 6px; padding: 4px 8px; }
QMenu { background: @panel@; border: 1px solid @hover@; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 6px 28px 6px 12px; border-radius: 6px; }
QMenu::item:selected { background: @selection@; color: @selectionText@; }
QMenu::item:disabled { color: @muted@; }
QMenu::separator { height: 1px; background: @hover@; margin: 4px 8px; }
QGroupBox { border: 1px solid rgba(255,255,255,0.10); border-radius: 12px; margin-top: 14px; padding: 8px 4px 4px 4px; font-weight: 700; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: @dim@; }
QLabel { color: @soft@; }
QAbstractSpinBox, QLineEdit, QComboBox {
    background: rgba(255,255,255,0.08); border: 1px solid transparent; border-radius: 8px; padding: 3px 6px; min-height: 20px;
    selection-background-color: @selection@; selection-color: @selectionText@; }
QAbstractSpinBox:focus, QLineEdit:focus, QComboBox:focus { border: 1px solid @accent@; }
QAbstractSpinBox:disabled, QLineEdit:disabled, QComboBox:disabled { color: @muted@; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { width: 14px; border: none; background: transparent; }
QAbstractSpinBox::up-arrow { image: none; width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent; border-bottom: 5px solid @dim@; }
QAbstractSpinBox::down-arrow { image: none; width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid @dim@; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox::down-arrow { image: none; width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid @dim@; margin-right: 6px; }
QComboBox QAbstractItemView { background: @panel@; border: 1px solid @hover@; border-radius: 8px; padding: 4px; selection-background-color: @selection@;
    selection-color: @selectionText@; outline: none; }
QCheckBox { spacing: 8px; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 5px; background: rgba(255,255,255,0.10); border: 1px solid rgba(255,255,255,0.14); }
QCheckBox::indicator:checked { background: @accent@; border-color: @accent@; }
QCheckBox::indicator:disabled { background: rgba(255,255,255,0.05); }
QPushButton { background: rgba(255,255,255,0.08); border: none; border-radius: 10px; padding: 6px 12px; min-height: 18px; font-weight: 600; }
QPushButton:hover { background: rgba(255,255,255,0.14); }
QPushButton:pressed { background: @accent@; color: @accentText@; }
QPushButton:disabled { color: @muted@; background: rgba(255,255,255,0.04); }
QSlider::groove:horizontal { height: 8px; border-radius: 4px; background: rgba(255,255,255,0.12); }
QSlider::sub-page:horizontal { background: @slider@; border-radius: 4px; }
QSlider::handle:horizontal { width: 16px; height: 16px; margin: -4px 0; border-radius: 8px; background: @text@; }
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
QTreeWidget::item:selected, QListWidget::item:selected { background: @selection@; color: @selectionText@; }
QTreeWidget::item:hover, QListWidget::item:hover { background: rgba(255,255,255,0.08); }
QHeaderView::section { background: transparent; color: @muted@; border: none; padding: 2px 4px; font-weight: 600; }
QTreeWidget::indicator { width: 14px; height: 14px; border-radius: 4px; background: rgba(255,255,255,0.10); }
QTreeWidget::indicator:checked { background: @emerald@; }
QTreeView::branch { background: transparent; }
)");
    if (!glassy()) {
        // A solid theme: thin borders where the glass has none.
        sheet += QStringLiteral(
            "QPushButton, QTreeWidget, QListWidget, QAbstractSpinBox, QLineEdit, QComboBox { border: 1px solid @edge@; }\n"
            "QPushButton:hover { border-color: @hover@; }\n"
            "QAbstractSpinBox:focus, QLineEdit:focus, QComboBox:focus { border-color: @accent@; }\n");
    }
    // The theme's tones (its ui and palette) into the sheet.
    const QColor selection = uiColor("selection"), selectionText = uiColor("selectionText"), edge = glassEdge();
    return sheet.replace(QStringLiteral("@accentText@"), uiColor("accentText").name())
        .replace(QStringLiteral("@accent@"), uiColor("accent").name())
        .replace(QStringLiteral("@selectionText@"), (selectionText.isValid() ? selectionText : uiColor("accentText")).name())
        .replace(QStringLiteral("@selection@"), (selection.isValid() ? selection : uiColor("accent")).name())
        .replace(QStringLiteral("@slider@"), (glassy() ? uiColor("accent") : textMuted()).name())
        .replace(QStringLiteral("@panel@"), uiColor("panel").name())
        .replace(QStringLiteral("@hover@"), uiColor("hover").name())
        .replace(QStringLiteral("@edge@"), edge.isValid() ? edge.name() : QStringLiteral("transparent"))
        .replace(QStringLiteral("@text@"), text().name())
        .replace(QStringLiteral("@soft@"), textSoft().name())
        .replace(QStringLiteral("@dim@"), textDim().name())
        .replace(QStringLiteral("@muted@"), textMuted().name())
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

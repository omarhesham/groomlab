// SPDX-License-Identifier: Apache-2.0
#pragma once

// The app's look: GroomLab's themes (hosts/maya/themes/*.json; Playful by default: charcoal and the
// primaries, round and friendly) as the standalone app wears them, "like Plasticity but Pixar".
// Everything floats over the viewport as translucent glass (or, for a theme with "glass": false
// such as Charcoal, solid panels with a thin border); this module holds the colors, the fonts
// (Figtree for the words, JetBrains Mono for the numbers, bundled as resources), the metrics
// and the layout of the floating pieces (pure geometry, so the tests check it without a
// window), the brush rail's order and the Qt style sheet the ordinary widgets inside the
// panels take. No widgets here: the painted ones are in glass.h.

#include <string>
#include <vector>

#include "groomlab/brush_catalog.h"

#include <QColor>
#include <QFont>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

namespace groomlab::app::theme {

// ---- The theme ------------------------------------------------------------------------------------
// The shipped themes are embedded at build time (resources :/groomlab/themes/<name>.json, the
// Maya host's files); one is current for the whole app. The widgets take their colors when they
// are made, so a theme is chosen before the window is built: main() sets the saved one (or
// --theme's), and View > Theme saves a new choice for the next start.
QStringList themeNames();                  // "charcoal", "playful", "polar", "studio" (sorted)
QString themeLabel(const QString& name);   // "Charcoal"
QString currentTheme();                    // "playful" until setTheme
bool setTheme(const QString& name);        // false (nothing changes) for a theme that isn't embedded
QString savedTheme();                      // QSettings ui/theme, "playful" when unset or unknown
void saveTheme(const QString& name);
bool glassy();  // the theme's ui.glass (default true): translucent panels, else solid ones with an edge

// ---- Colors (the palette, then the glass and text tones) -----------------------------------------
// The palette, the UI tones, the graph's and the icons' colors are read from the current theme's
// file, so the Maya host and the app share one theme file. A glass theme keeps the mockup's own
// glass and text tones (tuned for translucency over the viewport); a solid one takes them from
// its ui section (panel, border, text, textDim).
QColor red();
QColor yellow();
QColor blue();
QColor emerald();
QColor purple();
QColor white();
QColor glass();        // the floating panels: rgba(31,32,37,0.72); solid: ui.panel
QColor glassSolid();   // small labels that must stay readable: rgba(31,32,37,0.86); solid: ui.panel
QColor glassPanel();   // a working panel, a little more opaque: rgba(31,32,37,0.93); solid: ui.panel
QColor glassEdge();    // the solid panels' thin border (ui.outline, else ui.border); invalid for glass
QColor text();         // #F3F1EC; solid: ui.text
QColor textSoft();     // #D9D5CD, the inactive mode names
QColor textDim();      // #B8B4AC, slider captions
QColor textMuted();    // #8F8B84, shortcuts and counts; solid: ui.textDim
QColor track();        // a slider's unfilled track: white at 12%
QColor chip();         // a small button at rest: white at 8%
QColor divider();      // white at 12%
QColor renderRed();    // the Render pill: #E5383B; solid: the accent (the primary action)
QColor backdrop();     // the viewport's default clear color: #17181B; solid: ui.background
QColor ink();          // dark text on a bright fill: #1B1A17
QColor blueLight();    // icons tinted by category on glass: lighter than the fill colors
QColor redLight();
QColor emeraldSoft();  // the Sym X chip's text when on
QColor emeraldChip();  // ... and its fill
QColor readableOn(const QColor& fill);  // ink on a bright fill, white on a dark one
// A segmented control's chosen item: its own colors on glass; a solid theme's accent pill.
QColor segmentFill(const QColor& own);
QColor segmentText(const QColor& own);
// A slider's fill, as the Maya host's ui.sliders: a theme with one slider color (Charcoal's
// silver, Polar's and Studio's accent) gives every slider that color; a cycle (Playful's four
// primaries) keeps the color the slider asks for.
QColor sliderFill(const QColor& own);
// A filled action button or a checked chip (chrome, not meaning): its own colors on glass; on a
// solid theme the accent pill and the accent's text.
QColor actionFill(const QColor& own);
QColor actionText(const QColor& own);
// The theme's "ui" color of that key ("panel", "hover", "accent", "accentText"...); invalid for
// an unknown one.
QColor uiColor(const std::string& key);

// ---- Fonts -------------------------------------------------------------------------------------
// Registers the bundled fonts once (idempotent); false when the resources are missing, in
// which case uiFont falls back to the system's sans-serif.
bool loadFonts();
QString uiFamily();    // "Figtree"
QString monoFamily();  // "JetBrains Mono"
// A UI font of `pixelSize` and `weight` (QFont::Weight: 400 regular ... 800 extra bold).
QFont uiFont(int pixelSize, int weight = QFont::Medium);
QFont monoFont(int pixelSize, int weight = QFont::Medium);

// ---- Brushes --------------------------------------------------------------------------------------
// The rail's tools are the core's brush catalog (brush_catalog.h: the ids, labels, categories
// and hints both hosts share), colored as playful.json's icons are (create emerald, shape blue,
// trim red, paint yellow; the guide tools, Select and Freeze, white).
using BrushCategory = groomlab::BrushCategory;
struct BrushEntry {
    std::string name;  // the mode BrushToolSettings::setMode takes
    std::string icon;  // hosts/app/icons/groomLab_<icon>.svg
    std::string tip;   // "Comb: drag guide points along the stroke..."
    BrushCategory category;
};
// Every brush mode in the catalog's order, grouped by category (a divider between groups): the
// catalog without Select (the rail's own tool). Freeze is BrushSession::setTool's ("freeze").
const std::vector<BrushEntry>& brushes();
const BrushEntry* brush(const std::string& name);  // null for an unknown mode
QColor categoryColor(BrushCategory category);      // the fill (the active tool, the ring)
QColor categoryIconColor(BrushCategory category);  // the tint of an inactive tool's icon
QString brushTitle(const std::string& name);       // the catalog's label ("comb" -> "Comb"); a word capitalized otherwise

// ---- The node graph ------------------------------------------------------------------------------
// A node header's color by the operator's category ("source", "grow", "shape", "look",
// "field", "utility", "color", "motion", "feathers"; "" utility), as playful.json's graph section.
QColor nodeCategoryColor(const std::string& category);
// A port's color by its type name ("strands", "guides", "mesh", "field", "curve", "curves").
QColor portColor(const std::string& type);
QColor nodeBody();  // #2A2C31 (opaque: the groom shows between nodes, not through them)
QColor nodeBorder();       // graph.nodeBorder: drawn when it differs from the body
QColor nodeSelected();     // graph.selected: the selected node's ring and wire
qreal nodeRadius();        // graph.cornerRadius (14 in Playful)
bool tintedHeaders();      // graph.headerStyle "tinted": a wash and an accent bar, else filled

// ---- Layout --------------------------------------------------------------------------------------
// Where the floating pieces go over a view of a size, from the mockup's margins. The rail's
// buttons shrink (44 px down to 30) when the view is too short for every brush between the
// logo and the stats, so no brush ever scrolls away.
struct OverlayMetrics {
    int margin = 24;            // the panels' distance from the edges
    int topHeight = 44;         // the top row's buttons
    int railWidth = 56;         // the brush rail
    int railButton = 44;        // ... its buttons at most
    int railButtonMin = 30;
    int railGap = 4;            // between buttons
    int railDivider = 9;        // a divider's row (1 px line, 4 px margins)
    int railPadding = 8;        // the rail's top and bottom padding
    int panelButton = 48;       // the right rail's buttons
    int panelGap = 8;
    int barHeight = 56;         // the brush settings bar (12 px padding around 32 px controls)
    int barBottom = 17;         // ... its distance from the bottom
    int statsBottom = 34;       // the stats text's baseline distance from the bottom
    int lookCardWidth = 272;
    int lookCardTop = 92;
};
struct OverlayLayout {
    QRect logo;          // the logo and file name (top-left)
    QRect modes;         // the mode switcher pill (top-center), given its width
    QRect topRight;      // undo, redo, render (top-right), given its width
    QRect rail;          // the brush rail (left, centered vertically; it widens to the right on hover)
    int railButton = 0;  // the rail's button size after shrinking
    int railGap = 4;     // ... and its gaps and dividers (tightened when the window is short)
    int railDivider = 9;
    QRect panelRail;     // the panel buttons (right, centered vertically; they widen to the left on hover)
    QRect bar;           // the brush settings bar (bottom-center; right of the pills, or above them, when they would meet), given its width
    QRect stats;         // the stats text (bottom-left, above the pills; right of the rail when the rail needs the height), given its size
    QRect lookCard;      // the look panel (top-right), given its height
    QRect graphPill;     // the Graph pill (bottom-left, every mode), given its size
    QRect graphTools;    // the Presets pill after it (and the close button while the graph shows), given their width
    QRect refine;        // the refining stats (bottom-right), given its size
};
struct OverlayRequest {
    QSize view;
    int railButtons = 0;   // buttons in the brush rail
    int railDividers = 0;  // dividers between its groups
    int panelButtons = 0;  // buttons in the right rail
    QSize logo{200, 30};
    int modesWidth = 300;
    int topRightWidth = 200;
    int barWidth = 700;
    QSize stats{300, 16};
    int lookCardHeight = 320;
    QSize graphPill{170, 44};
    bool graphShown = false;   // the node graph is up: the close button joins the pills
    int graphToolsWidth = 0;   // the Presets pill (and, with the graph, the gap and the close button)
    bool barShown = true;      // the brush bar is up (Groom mode): it keeps clear of the pills
    QSize refine{160, 16};
};
OverlayLayout layoutOverlay(const OverlayRequest& request, const OverlayMetrics& metrics = OverlayMetrics());

// ---- Windows on small screens ---------------------------------------------------------------------
// A window's size for a screen: `wanted`, no more than `share` of the screen's usable area
// (QScreen::availableGeometry: logical pixels, so a 1920x1080 display at 150% text is 1280x720
// less the taskbar), and never below `minimum` unless the screen itself is smaller.
QSize fitToAvailable(QSize wanted, const QRect& available, double share = 0.92, QSize minimum = QSize(640, 400));
// (glass.h fitWindowToScreen applies it to a window.)

// ---- The style sheet ------------------------------------------------------------------------------
// What the ordinary widgets inside the glass panels look like (spin boxes, combos, check
// boxes, trees, scroll bars, menus, tool tips): flat, round, charcoal, the accent blue.
QString styleSheet();

}  // namespace groomlab::app::theme

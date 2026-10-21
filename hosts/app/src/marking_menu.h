// SPDX-License-Identifier: Apache-2.0
#pragma once

// The marking menu over the viewport, as Maya's GroomLab marking menu (hosts/maya/scripts/groomlab/
// ui/hotkeys.py) and Blender's pie, from engine/ui/marking_menu.json: the brushes reached for most
// at the eight compass points around the cursor (radial), the rest of the brush catalog in a list
// below them (overflow), then a divider and the tail (Select, Freeze, Groom Tools).
//
// Hold Y and press the left button over the viewport (Viewport::markingMenuPressed): the menu opens
// around the cursor. Drag toward an item and let go to pick it (a radial item by its direction once
// past the small dead zone in the middle; a list item under the cursor); let go without moving and
// the menu stays open, to click an item (a click in the middle or Esc closes it), as Maya's does.
//
// Painted with QPainter on a transparent child of the overlay host as big as the host (nothing
// behind the items is covered; it exists only while it shows), in the theme's tokens: solid panel
// pills with a thin edge, the hovered one filled with the tool's category color.
//
//     auto* menu = new MarkingMenu(host);
//     menu->setItems(AppActions::markingMenuItems(window));
//     menu->popup(at, true);          // a held button
//     menu->track(cursor);            // ... it moved
//     menu->release(cursor);          // ... let go: picked() when over an item

#include <string>
#include <vector>

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>

namespace groomlab::app {

class MarkingMenu : public QWidget {
    Q_OBJECT
public:
    struct Item {
        enum class Kind { Radial, Overflow, Tail };
        Kind kind = Kind::Overflow;
        std::string id;     // a tool's id ("comb", "select") or an action's ("tools.open")
        bool tool = true;   // false: an action of the definitions
        QString label, tip;
        QString icon;       // an icon resource ("" none)
        QColor color;       // the hovered fill (a tool's category color); invalid: the theme's
        int position = -1;  // Radial: 0 N, 1 NE, 2 E ... 7 NW (marking_menu.json's position)
    };
    // The compass point's index (N 0 ... NW 7), -1 for an unknown name.
    static int positionIndex(const std::string& name);

    explicit MarkingMenu(QWidget* host);  // covers the host, hidden

    void setItems(std::vector<Item> items);
    const std::vector<Item>& items() const { return items_; }

    // Shown with its middle at `at` (the host's coordinates). `held`: a button is down, so a drag
    // and its release pick (track, release); else it waits for a click.
    void popup(QPointF at, bool held);
    void track(QPointF at);    // the held button moved (the host's coordinates)
    void release(QPointF at);  // ... and was let go: picks what is under it, or stays open (no move)
    void dismiss();
    bool shown() const { return isVisible(); }
    bool held() const { return held_; }

    // The layout (this widget's coordinates, the same as the host's): the middle, each item's box.
    QPointF center() const { return center_; }
    QRectF itemRect(int index) const;
    // The item under `at` (the host's coordinates): a list row or a pill under it, else a radial
    // item by the direction from the middle past the dead zone; -1 none.
    int itemAt(QPointF at) const;
    int hovered() const { return hovered_; }
    static constexpr qreal kDeadZone = 18.0;

signals:
    void picked(const MarkingMenu::Item& item);
    void dismissed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;  // the host's resize

private:
    void layoutItems();
    void hover(QPointF at);
    void pick(int index);

    std::vector<Item> items_;
    std::vector<QRectF> rects_;  // per item
    QRectF listRect_;            // the overflow and tail's panel
    qreal dividerY_ = -1.0;      // the line above the tail (-1 none)
    QPointF center_, cursor_, pressAt_;  // the middle (moved in near the edges), the pointer, where it opened
    int hovered_ = -1;
    bool held_ = false;
    bool moved_ = false;  // the held button went past the dead zone from where it was pressed
};

}  // namespace groomlab::app

// SPDX-License-Identifier: Apache-2.0
#pragma once

// The Groom Tools' Photos tab, as Maya's tool_tabs.py ReferenceTab lays it out (engine/ui/lists.json's
// photos: the about line, the bar, the right-click menus): reference photos to groom to, each a
// camera.
//
//   list          the photos (a thumbnail, the name, the size, locked); double-click: Look Through
//   Look Through  the viewport shows the photo's camera with the photo fitted in the frame (again:
//                 back to the camera it showed before); Lock keeps the camera where it is (tumbling
//                 through it does nothing; unlocked, tumbling lines it up by eye)
//   Opacity, Photo (In front / Behind) and the eye: how the photo shows while looking through it
//   Align by Points  the photo's point pairs (a feature on the photo and the same feature on the
//                 mesh; picked side by side in Guides from Photos: Pick Points...), each pair's
//                 error, Solve (4 pairs keeping the focal length, else 6), Clear Pairs, Delete Pair,
//                 right-click: pick a point again
//   Guides from Photos  the Reference window (reference_window.h) on the picked photo
//
// The photos are the groom's (GroomDocument::referencePhotos, its file's "referencePhotos": the
// same records the Reference window and Blender's Photos edit), each with its "display" (opacity,
// depth, locked, visible). The window (MainWindow) owns the viewport: the panel asks
// (lookThroughRequested, displayChanged) and the window tells it back (setLookingThrough).

#include <optional>
#include <set>
#include <string>

#include <QImage>
#include <QMap>
#include <QString>
#include <QWidget>

#include <nlohmann/json.hpp>

#include "definitions.h"
#include "groomlab/reference_guides.h"

class QCheckBox;
class QLabel;
class QListWidget;
class QMenu;
class QPushButton;
class QToolButton;
class QTreeWidget;

namespace groomlab::app {

class GroomDocument;
class ListBar;
class ValueSlider;

class PhotosPanel : public QWidget {
    Q_OBJECT
public:
    explicit PhotosPanel(QWidget* parent = nullptr);

    void setDocument(GroomDocument* document);  // null: nothing to list
    // The window's bindings: the bars (photos, pairs, guides) and the right-click menus.
    void setActions(const ui::Bindings& bindings, const ui::FillOptions& options);
    void refresh();  // the list and the picked photo's controls from the document

    // ---- The photos ------------------------------------------------------------------------------
    int count() const;
    int current() const;  // the picked photo, or -1
    void select(int index);
    QString name(int index) const;
    QString imagePath(int index) const;
    QImage picture(int index) const;  // read once per file (and when it changes)
    QSize imageSize(int index) const;
    // The photo's camera (stored, else none: never lined up).
    std::optional<RefCamera> camera(int index) const;
    void setCamera(int index, const RefCamera& camera);  // stored with the groom
    // A camera for a photo never lined up (Import Photo's, Maya's reference._place): in front (+Z)
    // of the groom, far enough for it to fill most of the photo's short side, a 50 mm lens.
    RefCamera placedCamera(int width, int height) const;
    bool placeCamera(int index);  // that camera stored for photo `index`
    // How it shows while looking through it.
    struct Display {
        double opacity = 0.6;  // Maya's reference.DEFAULT_OPACITY
        bool front = true;     // choices.json photoDepth: front (over the mesh) or behind
        bool locked = false;
        bool visible = true;
    };
    Display display(int index) const;
    void setDisplay(int index, const Display& display);

    // ---- Edits (the bindings and the tests) -------------------------------------------------------
    // A photo from a file: its size read, named after it (made unique), its camera placed in front
    // of the groom (+Z) far enough for it to fill the photo's short side (Maya's _place), picked.
    // Its index, or -1 (why: message()).
    int importPhoto(const QString& path);
    bool rename(int index, const QString& name);  // false: empty, taken or no such photo
    bool remove(int index);
    // Align by Points: the picked photo's pairs.
    int pairCount() const;      // pairs with both points, and pending ones
    int pickedPair() const;     // the pairs table's row, or -1
    void pickPair(int row);
    bool solve();               // the camera from the complete pairs; false (why: message) without enough
    void clearPairs();
    bool deletePair(int row);
    // Clear the pair's point on the photo (`photoSide`) or on the mesh: its other point waits at the
    // end of the list for a new partner (picked in Guides from Photos).
    bool repickPoint(int row, bool photoSide);
    void setKeepFocal(bool keep);
    bool keepFocal() const;
    QString solveText() const;  // the last solve's line ("" none)

    // ---- Looking through ---------------------------------------------------------------------------
    // The photo the viewport looks through ("" none): the buttons follow (the window says).
    void setLookingThrough(const QString& name);
    const QString& lookingThrough() const { return looking_; }
    int lookingIndex() const;

    // The right-click menus (not shown: the caller pops them up; they delete themselves).
    QMenu* contextMenu(const QPoint& pos);
    QMenu* pairsMenu(const QPoint& pos);
    std::set<std::string> menuState() const;  // "photo" with one picked

    // ---- The pieces (tests) ------------------------------------------------------------------------
    QListWidget* list() const { return list_; }
    QPushButton* lookButton() const { return look_; }
    QToolButton* lockButton() const { return lock_; }
    ValueSlider* opacitySlider() const { return opacity_; }
    QToolButton* depthButton(bool front) const { return front ? front_ : behind_; }
    QToolButton* visibleButton() const { return visible_; }
    QTreeWidget* pairsTable() const { return pairs_; }
    ListBar* bar() const { return bar_; }
    ListBar* pairsBar() const { return pairsBar_; }

signals:
    void lookThroughRequested(int index, bool on);
    void displayChanged();  // the opacity, the side, the visibility or the lock of a photo
    void edited();          // the photos changed (stored with the groom)
    void pickPointsRequested(int index);
    void message(const QString& text);

private:
    nlohmann::json photos() const;  // the document's records (an array)
    void store(const nlohmann::json& photos);
    void refreshControls();
    void refreshPairs();
    QString uniqueName(const QString& wanted, int except = -1) const;

    GroomDocument* document_ = nullptr;
    ui::Bindings bindings_;
    ui::FillOptions options_;
    QString looking_;
    bool updating_ = false;
    mutable QMap<QString, QImage> pictures_;  // by file
    QString solveText_;
    QLabel* about_ = nullptr;
    ListBar* bar_ = nullptr;
    QListWidget* list_ = nullptr;
    QPushButton* look_ = nullptr;
    QToolButton* lock_ = nullptr;
    ValueSlider* opacity_ = nullptr;
    QToolButton* front_ = nullptr;
    QToolButton* behind_ = nullptr;
    QToolButton* visible_ = nullptr;
    QLabel* alignAbout_ = nullptr;
    QToolButton* pick_ = nullptr;
    QCheckBox* keepFocal_ = nullptr;
    QTreeWidget* pairs_ = nullptr;
    ListBar* pairsBar_ = nullptr;
    QLabel* result_ = nullptr;
    QLabel* guidesHint_ = nullptr;
    ListBar* guidesBar_ = nullptr;
};

}  // namespace groomlab::app

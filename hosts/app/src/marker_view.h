// SPDX-License-Identifier: Apache-2.0
#pragma once

// What the app's Wrap-style windows share (Tools > Transfer Groom to Mesh: two mesh views; Tools >
// Guides from Reference: a photo and a mesh view): a view of one mesh with its own orbit camera
// (Alt+left orbits, Alt+middle or middle pans, Alt+right, right or the wheel dollies; Frame fits)
// and one side of a window's marker pairs drawn over it as numbered dots, and the dots' drawing.
//
// The pairs and their edits are the window's (MarkerViewHost): the core's MarkerLists rules
// (transfer_markers_edit.h): a click adds a point at the end of the view's side (a ray against
// the mesh: ScalpMesh::intersectRay, as the brushes pick), a drag on a point slides it over the
// surface, Ctrl+click on a point deletes it and its partner, hovering a point lights it and its
// partner. Unmatched points draw hollow, matched ones filled; numbers are the pair index from 1.
//
// The view renders the mesh with the app's GroomRenderer (no lookdev, no strands) into an
// OffscreenView on a QRhi its window shares between its views, read back into an image the widget
// paints with the dots over it: one path on screen, in the offscreen tests and in a grab().

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QWidget>

#include "camera.h"
#include "groomlab/transfer_markers_edit.h"
#include "groomlab/view_projection.h"

class QPainter;
class QRhi;

namespace groomlab::app {

class GroomRenderer;
class OffscreenView;

using MarkerHover = std::optional<std::pair<MarkerSide, std::size_t>>;

// The window a MarkerView belongs to: its pairs, their edits and the shared GPU device.
class MarkerViewHost {
public:
    virtual ~MarkerViewHost() = default;
    virtual const MarkerLists& markerLists() const = 0;
    virtual bool markerEditing() const = 0;  // clicks place points (else a left drag orbits)
    virtual const MarkerHover& hover() const = 0;
    virtual void setHover(MarkerHover hover) = 0;
    virtual std::size_t addPicked(MarkerSide side, const MarkerPoint& picked) = 0;
    virtual std::vector<std::size_t> deletePoint(MarkerSide side, std::size_t index) = 0;
    virtual void movePicked(MarkerSide side, std::size_t index, const MarkerPoint& picked, bool newStep) = 0;
    virtual void edited(bool store = true) = 0;  // a finished drag
    virtual QRhi* rhi() = 0;
    virtual QColor markerColor(MarkerSide side) const;
    virtual QString emptyViewText(MarkerSide side) const;
};

// A numbered marker dot as every view draws it: filled when matched, a ring pending, faint
// (behind the mesh), a white ring when lit; `color` the side's (or a bad pair's red).
void paintMarkerDot(QPainter& painter, const QPointF& at, int number, const QColor& color, bool matched, bool lit,
                    bool faint = false);
// The side colors: yellow (the left view's side), cyan (the right view's).
QColor markerSideColor(MarkerSide side);

// One side's view: its mesh, its camera, the side's points as dots.
class MarkerView : public QWidget {
    Q_OBJECT
public:
    MarkerView(MarkerViewHost* host, MarkerSide side, QWidget* parent = nullptr);
    ~MarkerView() override;

    MarkerSide side() const { return side_; }
    // The mesh shown (null: none); a new one is framed.
    void setMesh(std::shared_ptr<const ScalpMesh> mesh);
    const std::shared_ptr<const ScalpMesh>& mesh() const { return mesh_; }
    void frame();
    OrbitCamera& camera() { return camera_; }
    const OrbitCamera& camera() const { return camera_; }
    // The camera moved (or the mesh changed): rendered again on the next paint.
    void cameraChanged();

    // The camera over this widget, as the core's tools read a view.
    ToolView toolView() const;
    // The mesh under a widget position (y down) as a marker point; false on a miss.
    bool pick(const QPointF& position, MarkerPoint* hit) const;
    // A world point in widget pixels; false behind the camera.
    bool project(const Vec3f& p, QPointF* position) const;
    // The side's point projected nearest `position` within `pixels`, or -1.
    int pointNear(const QPointF& position, float pixels = 10.0f) const;
    // Whether the mesh hides a point of it from the camera (drawn faint).
    bool hidden(const Vec3f& p) const;
    // The mesh rendered at the widget's size (null without a GPU device).
    QImage meshImage();
    // The renderer's GPU objects released (before the window's QRhi goes).
    void releaseGpu();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    enum class Drag { None, Orbit, Pan, Dolly, Point, Owned };
    MarkerViewHost* host_;
    MarkerSide side_;
    std::shared_ptr<const ScalpMesh> mesh_;
    OrbitCamera camera_;
    std::unique_ptr<GroomRenderer> renderer_;
    std::unique_ptr<OffscreenView> target_;
    QImage image_;
    bool dirty_ = true;
    Drag drag_ = Drag::None;
    QPoint last_;
    std::size_t dragIndex_ = 0;
    bool firstMove_ = false;
};

}  // namespace groomlab::app

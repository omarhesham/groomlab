// SPDX-License-Identifier: Apache-2.0
#include "marking_menu.h"

#include <algorithm>
#include <cmath>

#include <QEvent>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "glass.h"
#include "theme.h"

namespace groomlab::app {

namespace {

constexpr const char* kPositions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
constexpr qreal kPi = 3.14159265358979323846;
// The pills' anchors from the middle: across (E, W) and up and down (N, S); the diagonals between.
constexpr qreal kReachX = 92.0, kReachY = 70.0;
constexpr qreal kPillHeight = 28.0, kRowHeight = 26.0, kIcon = 16.0, kPad = 11.0, kGap = 7.0;
constexpr qreal kListGap = 34.0;  // the list's top below the S pill
constexpr qreal kMargin = 8.0;    // from the host's edges

// A compass point's direction (y down): N up, then clockwise.
QPointF direction(int position) {
    const qreal a = position * kPi / 4.0;
    return {std::sin(a), -std::cos(a)};
}

QFont labelFont() { return theme::uiFont(12, QFont::Medium); }

QColor hoverFill(const MarkingMenu::Item& item) { return item.color.isValid() ? item.color : theme::chipChecked(); }

}  // namespace

int MarkingMenu::positionIndex(const std::string& name) {
    for (int i = 0; i < 8; ++i) {
        if (name == kPositions[i]) return i;
    }
    return -1;
}

MarkingMenu::MarkingMenu(QWidget* host) : QWidget(host) {
    setObjectName(QStringLiteral("markingMenu"));
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    if (host) {
        setGeometry(host->rect());
        host->installEventFilter(this);
    }
    hide();
}

void MarkingMenu::setItems(std::vector<Item> items) {
    items_ = std::move(items);
    hovered_ = -1;
    layoutItems();
    update();
}

void MarkingMenu::layoutItems() {
    rects_.assign(items_.size(), QRectF());
    listRect_ = QRectF();
    dividerY_ = -1.0;
    const QFontMetricsF fm(labelFont());
    auto pillWidth = [&](const Item& item) {
        return std::ceil(fm.horizontalAdvance(item.label) + 2.0 * kPad + (item.icon.isEmpty() ? 0.0 : kIcon + kGap));
    };
    // The radial pills around the middle: N and S centered over and under it, the east side's
    // starting at their anchors, the west side's ending at theirs (laid out around 0, 0 first).
    QRectF radial(QPointF(-kDeadZone, -kDeadZone), QPointF(kDeadZone, kDeadZone));
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const Item& item = items_[i];
        if (item.kind != Item::Kind::Radial || item.position < 0) continue;
        const QPointF d = direction(item.position);
        const QPointF anchor(d.x() * kReachX, d.y() * kReachY);
        const qreal w = pillWidth(item);
        qreal left = anchor.x() - w / 2.0;
        if (d.x() > 0.3) left = anchor.x() - 10.0;
        else if (d.x() < -0.3) left = anchor.x() - w + 10.0;
        rects_[i] = QRectF(left, anchor.y() - kPillHeight / 2.0, w, kPillHeight);
        radial = radial.united(rects_[i]);
    }
    // Near the host's edges the middle moves in so every pill shows (as Blender's pie does); the
    // directions are from where it is drawn.
    const QRectF area = QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);
    if (area.width() > radial.width() && area.height() > radial.height()) {
        center_.setX(std::clamp(center_.x(), area.left() - radial.left(), area.right() - radial.right()));
        center_.setY(std::clamp(center_.y(), area.top() - radial.top(), area.bottom() - radial.bottom()));
    }
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (rects_[i].isValid()) rects_[i].translate(center_);
    }
    const qreal radialBottom = center_.y() + std::max(radial.bottom(), kPillHeight / 2.0);
    const qreal radialRight = center_.x() + radial.right(), radialLeft = center_.x() + radial.left();
    // The list: one panel of rows (the overflow, a divider, the tail) under the S pill; to the side
    // when it does not fit under it.
    std::vector<std::size_t> rows;
    qreal listWidth = 0.0;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].kind == Item::Kind::Radial) continue;
        rows.push_back(i);
        listWidth = std::max(listWidth, pillWidth(items_[i]) + 8.0);
    }
    if (rows.empty()) return;
    bool tail = false;
    qreal listHeight = 8.0;
    for (std::size_t i : rows) {
        if (items_[i].kind == Item::Kind::Tail && !tail) {
            tail = true;
            if (i != rows.front()) listHeight += 9.0;
        }
        listHeight += kRowHeight;
    }
    listHeight += 8.0 - 4.0;
    QPointF topLeft(center_.x() - listWidth / 2.0, radialBottom + kListGap - kPillHeight / 2.0);
    if (topLeft.y() + listHeight > area.bottom() && area.height() > 0.0) {
        // Beside the radial items instead (the right, else the left), as low as it fits.
        topLeft.setX(radialRight + 16.0);
        if (topLeft.x() + listWidth > area.right()) topLeft.setX(radialLeft - 16.0 - listWidth);
        topLeft.setY(std::max(area.top(), std::min(center_.y() - kReachY - kPillHeight / 2.0, area.bottom() - listHeight)));
    }
    if (area.width() > 0.0) topLeft.setX(std::clamp(topLeft.x(), area.left(), std::max(area.left(), area.right() - listWidth)));
    listRect_ = QRectF(topLeft, QSizeF(listWidth, listHeight));
    qreal y = listRect_.top() + 4.0;
    tail = false;
    for (std::size_t i : rows) {
        if (items_[i].kind == Item::Kind::Tail && !tail) {
            tail = true;
            if (i != rows.front()) {
                dividerY_ = y + 4.0;
                y += 9.0;
            }
        }
        rects_[i] = QRectF(listRect_.left() + 4.0, y, listWidth - 8.0, kRowHeight);
        y += kRowHeight;
    }
}

QRectF MarkingMenu::itemRect(int index) const {
    return index >= 0 && static_cast<std::size_t>(index) < rects_.size() ? rects_[static_cast<std::size_t>(index)] : QRectF();
}

int MarkingMenu::itemAt(QPointF at) const {
    for (std::size_t i = 0; i < rects_.size(); ++i) {
        if (rects_[i].contains(at)) return static_cast<int>(i);
    }
    if (listRect_.contains(at)) return -1;  // the list's padding or divider
    const QPointF d = at - center_;
    const qreal length = std::hypot(d.x(), d.y());
    if (length < kDeadZone) return -1;
    // The radial item whose direction is closest (within the eighth around it).
    int best = -1;
    qreal bestDot = std::cos(kPi / 8.0) - 1e-6;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].kind != Item::Kind::Radial || items_[i].position < 0) continue;
        const QPointF dir = direction(items_[i].position);
        const qreal dot = (d.x() * dir.x() + d.y() * dir.y()) / length;
        if (dot > bestDot) {
            bestDot = dot;
            best = static_cast<int>(i);
        }
    }
    return best;
}

void MarkingMenu::popup(QPointF at, bool held) {
    if (parentWidget()) setGeometry(parentWidget()->rect());
    center_ = cursor_ = pressAt_ = at;
    held_ = held;
    moved_ = false;
    hovered_ = -1;
    layoutItems();
    show();
    raise();
    if (!held) setFocus(Qt::PopupFocusReason);
    update();
}

void MarkingMenu::hover(QPointF at) {
    cursor_ = at;
    const int now = itemAt(at);
    if (now != hovered_) hovered_ = now;
    update();
}

void MarkingMenu::track(QPointF at) {
    if (!isVisible()) return;
    if (QLineF(at, pressAt_).length() >= kDeadZone) moved_ = true;
    hover(at);
}

void MarkingMenu::release(QPointF at) {
    if (!isVisible() || !held_) return;
    held_ = false;
    const int index = itemAt(at);
    if (index >= 0 && (moved_ || rects_[static_cast<std::size_t>(index)].contains(at))) {
        pick(index);
        return;
    }
    if (moved_) {
        dismiss();  // let go over nothing
        return;
    }
    // A click: the menu stays for the next one.
    setFocus(Qt::PopupFocusReason);
    hover(at);
}

void MarkingMenu::pick(int index) {
    const Item item = items_[static_cast<std::size_t>(index)];
    dismiss();
    emit picked(item);
}

void MarkingMenu::dismiss() {
    if (!isVisible()) return;
    held_ = false;
    hovered_ = -1;
    hide();
    if (parentWidget()) parentWidget()->update();
    emit dismissed();
}

void MarkingMenu::mousePressEvent(QMouseEvent* event) {
    event->accept();
    if (event->button() != Qt::LeftButton) {
        dismiss();
        return;
    }
    const int index = itemAt(event->position());
    if (index >= 0) pick(index);
    else dismiss();
}

void MarkingMenu::mouseMoveEvent(QMouseEvent* event) {
    event->accept();
    if (!held_) hover(event->position());
}

void MarkingMenu::mouseReleaseEvent(QMouseEvent* event) { event->accept(); }

void MarkingMenu::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        dismiss();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool MarkingMenu::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
        if (isVisible()) layoutItems();
    }
    return QWidget::eventFilter(watched, event);
}

void MarkingMenu::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setFont(labelFont());
    const qreal dpr = devicePixelRatioF();
    const QColor panel = theme::glassPanel(), edge = theme::glassEdge().isValid() ? theme::glassEdge() : theme::divider();
    const QColor shadow(0, 0, 0, 90);
    const qreal radius = 7.0;

    // The middle: a small ring, and the line to the cursor while it points somewhere.
    QColor guide = theme::text();
    guide.setAlphaF(0.55f);
    if (QLineF(cursor_, center_).length() >= kDeadZone) {
        QColor line = guide;
        line.setAlphaF(0.35f);
        p.setPen(QPen(line, 1.5, Qt::SolidLine, Qt::RoundCap));
        const QPointF d = cursor_ - center_;
        const qreal len = std::hypot(d.x(), d.y());
        p.drawLine(center_ + d * (6.0 / len), cursor_);
    }
    p.setPen(QPen(guide, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(center_, 5.0, 5.0);

    auto drawItem = [&](const Item& item, const QRectF& r, bool hovered, bool pill) {
        const QColor fill = hovered ? hoverFill(item) : Qt::transparent;
        const QColor text = hovered ? theme::readableOn(hoverFill(item)) : theme::text();
        if (pill) {
            p.setPen(Qt::NoPen);
            p.setBrush(shadow);
            p.drawRoundedRect(r.translated(0.0, 2.0), radius, radius);
            p.setBrush(hovered ? fill : panel);
            p.setPen(QPen(hovered ? fill : edge, 1.0));
            p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        } else if (hovered) {
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawRoundedRect(r, 5.0, 5.0);
        }
        qreal x = r.left() + kPad;
        if (!item.icon.isEmpty()) {
            const QColor tint = hovered ? text : (item.color.isValid() ? item.color.lighter(125) : theme::textDim());
            const QPixmap icon = tintedIcon(item.icon, tint, static_cast<int>(kIcon), dpr);
            p.drawPixmap(QPointF(x, r.center().y() - kIcon / 2.0), icon);
            x += kIcon + kGap;
        }
        p.setPen(text);
        p.drawText(QRectF(x, r.top(), r.right() - x - kPad + 2.0, r.height()), Qt::AlignVCenter | Qt::AlignLeft, item.label);
    };

    if (listRect_.isValid()) {
        p.setPen(Qt::NoPen);
        p.setBrush(shadow);
        p.drawRoundedRect(listRect_.translated(0.0, 3.0), radius + 1.0, radius + 1.0);
        p.setBrush(panel);
        p.setPen(QPen(edge, 1.0));
        p.drawRoundedRect(listRect_.adjusted(0.5, 0.5, -0.5, -0.5), radius + 1.0, radius + 1.0);
        if (dividerY_ >= 0.0) {
            p.setPen(QPen(theme::divider(), 1.0));
            p.drawLine(QPointF(listRect_.left() + 10.0, dividerY_), QPointF(listRect_.right() - 10.0, dividerY_));
        }
    }
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (!rects_[i].isValid()) continue;
        drawItem(items_[i], rects_[i], static_cast<int>(i) == hovered_, items_[i].kind == Item::Kind::Radial);
    }
}

}  // namespace groomlab::app

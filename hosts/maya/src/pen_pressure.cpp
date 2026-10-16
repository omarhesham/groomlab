// SPDX-License-Identifier: Apache-2.0
#include "pen_pressure.h"

#include <algorithm>
#include <atomic>

#if GROOMLAB_HAS_QT
#include <QtCore/QObject>
#include <QtGui/QMouseEvent>
#include <QtGui/QTabletEvent>
#include <QtWidgets/QApplication>
#endif

namespace groomlab::maya {

namespace {

std::atomic<float> gPressure{1.0f};

#if GROOMLAB_HAS_QT
// Watches every event without consuming any (no Q_OBJECT: no signals or slots needed).
class PressureFilter : public QObject {
public:
    bool eventFilter(QObject* watched, QEvent* event) override {
        switch (event->type()) {
            case QEvent::TabletPress:
            case QEvent::TabletMove:
                gPressure = static_cast<float>(static_cast<QTabletEvent*>(event)->pressure());
                break;
            case QEvent::MouseButtonPress:
                // A real mouse (not one synthesized from the pen) brushes at full strength.
                if (static_cast<QMouseEvent*>(event)->source() == Qt::MouseEventNotSynthesized) gPressure = 1.0f;
                break;
            default:
                break;
        }
        return QObject::eventFilter(watched, event);
    }
};

PressureFilter* gFilter = nullptr;
#endif

}  // namespace

void installPenPressure() {
#if GROOMLAB_HAS_QT
    if (gFilter || !qApp) return;
    gFilter = new PressureFilter();
    qApp->installEventFilter(gFilter);
#endif
}

void removePenPressure() {
#if GROOMLAB_HAS_QT
    if (!gFilter) return;
    if (qApp) qApp->removeEventFilter(gFilter);
    delete gFilter;
    gFilter = nullptr;
#endif
    gPressure = 1.0f;
}

float penPressure() { return std::clamp(gPressure.load(), 0.05f, 1.0f); }

}  // namespace groomlab::maya

// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace groomlab::maya {

// Tablet pen pressure for the interactive brush. Maya's MEvent carries no
// pressure, so a Qt event filter on the application records the last tablet
// event's pressure; a mouse press resets it to 1. Without Qt headers at build
// time the pressure is always 1.
void installPenPressure();  // idempotent; call when a tool activates
void removePenPressure();   // before the plugin unloads
float penPressure();        // (0, 1]; 1 for the mouse

}  // namespace groomlab::maya

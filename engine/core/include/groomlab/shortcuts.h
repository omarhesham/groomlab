// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <vector>

namespace groomlab {

// The keyboard and mouse shortcuts of every host, in one table: Maya's Help > Keyboard Shortcuts
// and its Graph Editor menu keys, the app's Help > Keyboard Shortcuts sheet (and the docs page
// built from it), Blender's shortcut popup. A row names the hosts it holds in; a key that differs
// between hosts is one row per host.
enum ShortcutHost : unsigned { kShortcutMaya = 1u, kShortcutApp = 2u, kShortcutBlender = 4u, kShortcutAll = 7u };

struct Shortcut {
    std::string id;       // a stable name hosts look a key up by ("graph.frame"), unique per host
    std::string context;  // "File", "Modes", "Brush", "Viewport", "Graph"
    std::string keys;     // "B + drag", "Ctrl+S / Ctrl+Shift+S"
    std::string action;   // what it does, a sentence fragment
    unsigned hosts = kShortcutAll;
    // Maya: bound by GroomLab > Install GroomLab Hotkeys (the GroomLab hotkey set), not by default.
    bool mayaHotkeySet = false;
};

// Every row, contexts in the order above.
const std::vector<Shortcut>& shortcuts();

// The rows a host shows (kShortcutMaya, kShortcutApp or kShortcutBlender).
std::vector<Shortcut> shortcutsFor(unsigned host);

// The keys of row `id` for a host ("" when it has none there), e.g. for a tooltip's "(F)".
std::string shortcutKeys(const std::string& id, unsigned host);

}  // namespace groomlab

# UI definitions: one UI structure for every host

`engine/ui/*.json` says what GroomLab's menus, panels and lists hold, so Maya, the standalone app
and the Blender add-on show the same things in the same order with the same words. The files are
plain JSON (Apache-2.0 data): a host reads them with no engine loaded. Each host keeps its own
layout and widgets and binds the files' **action ids** to what they run.

| File | Holds |
| --- | --- |
| `actions.json` | Every action: `label`, `tip`, and optionally `shortcut` (a `shortcuts.h` id), `shortcutPart`, `icon` (an icon stem), `checkable`, `hosts`, `needs`, `enabled`, `tipDisabled`. |
| `menus.json` | The GroomLab menu (`main`), its submenus (`samples`, `import`, `export`, `render`, `studio`, `theme`, `help`) and the Groom Tools' Output lists (`output.import`, `output.export`...). |
| `palette.json` | The Groom Tools: tabs (with old ids as `aliases`), header tips and its More menu, the empty state, sections, the Output strip, the first-groom steps, texts (gestures, symmetry tip, erase note, flow hints). |
| `lists.json` | Each list's action bar (`bar`: entries with a `glyph` go on the right) and right-click menus: guide groups (and Edit Guides), parts, maps, correctives, photos and point pairs. |
| `choices.json` | Option lists: `{label, tip, default, items: [{value, label, tip}]}` (flow sinks, symmetry, sculpt display, tube channels and shapes, map kinds, values, resolutions, scalp and paint channels, photo depth, strand colors, evaluation). |
| `marking_menu.json` | The marking menu / pie: `radial` (position, tool), `overflow` (the rest of the brush catalog after a divider), `tail`. |
| `graph_ui.json` | The Graph Editor: category, port and key-state colors (a theme's `graph.categories` and `graph.ports` override them), backdrop colors, plumbing params, file filters, and the node, backdrop, Edit, Add Node, Presets, param, key and Paint menus. |
| `ui.schema.json` | The JSON Schema every file follows (each file holds some of its top-level keys). |

## Entries

A container is a list of entries (or `{label, tip, items}`):

- `"-"`: a divider (kept only between items).
- `"export.alembic"`: an action, as `actions.json` has it.
- `{"id": "file.saveAs", "tip": "..."}`: an action with this place's own label, tip, `glyph`, `needs` or `hosts`.
- `{"submenu": "Export", "id": "export", "items": [...]}` or `{"submenu": ..., "ref": "menus/export"}`: a submenu; `{"ref": "menus/theme"}` takes the container's label and tip too.
- `{"include": "lists/maps/image"}`: another list's entries, in place.
- `{"generator": "name", "action": "id", ...}`: entries made at run time. `choices` (from
  `choices.json`, the option's value passed to the action) and `backdropColors` expand from the
  files; the rest are the host's: `studioPresets`, `themes`, `themeMenu` (the Theme submenu),
  `units`, `nodeCategories`, `presets`, `exposeParams`, `innerParams`, `paramControl`.

`hosts` (`maya`, `blender`, `app`) limits an entry or action to those hosts (none: every host).
`needs` lists conditions the host knows (`map`, `map.texture`, `!map.color`, `node.compound`,
`param.linkable`, `group`, `photo`, `catalog`, `groom`...): an entry whose conditions do not
hold is left out (an action bar shows everything). `enabled` lists conditions for the enabled state
instead (`tipDisabled` is the tip then; `{why}` and other `{name}`s are filled from the host's values).
Placements are the same id listed in two containers (the GroomLab menu's `export` and the Output
strip's `output.export`).

## Binding the ids

Maya (`hosts/maya/scripts/groomlab/ui/definitions.py`) loads the files from the module's `ui/`
folder (installed beside `themes/`), else from the source tree's `engine/ui`, and resolves a
container for the host and a state (`definitions.resolve`). Each window keeps a table `{action id:
binding}`: a callable, a method name, or a function of the owner (`menu.ACTIONS`,
`ToolPalette.ACTIONS`, `GuidesTab.ACTIONS`, `NodeItem.ACTIONS`, `ParamPanel.param_actions`...);
`widgets.fill_menu` builds a Qt menu from a container and a table, `menu._build` the GroomLab menu
with `cmds.menuItem`. `maya.ui_scaling` checks that every id a Maya container lists has a binding,
and `ui.definitions` checks the files themselves (the schema, every id and reference, every action
used somewhere, and, with `groomlab_core` importable, the shortcut ids and tools against the core).

The app gets the files embedded as Qt resources (`:/groomlab/ui/menus.json`), the Blender extension
as plain files in `GroomLabBlender/ui/` (`hosts/blender/addon/CMakeLists.txt`). Blender reads them
with `hosts/blender/addon/groomlab/definitions.py` (Maya's semantics, host `blender`) and binds the
ids in `actions.py` (`BINDINGS`: action id -> operator idname and properties); an id without a
binding is hidden, and `blender.addon_ui` fails on any id Blender shows that is neither bound nor in
its `ALLOWED_MISSING` list (the parity checklist). A new host action:
add it to `actions.json`, list it in a container, bind it in each host that shows it (or give it
`hosts`), and run `ctest -R "ui.definitions|maya.ui_scaling"`.

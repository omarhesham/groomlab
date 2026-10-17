# SPDX-License-Identifier: Apache-2.0
"""Pictures of GroomLab's Maya windows in each theme, headless: mayapy (its PySide2), Qt offscreen
and the stand-in Maya the maya.ui_scaling test uses; no window on the desktop, no Maya session.

    mayapy tools/dev/theme_previews.py --core <folder of groomlab_core.pyd> --out <folder>
        [--dll-dir <dir>]... [--theme charcoal --theme playful ...]

Writes <out>/maya_groom_tools_<theme>.png, maya_graph_editor_<theme>.png and
maya_theme_menu_<theme>.png for every theme asked for (default: every shipped one).
"""

import argparse
import os
import sys
import tempfile
import traceback

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tests", "hosts", "maya"))

import qt_compat_check  # noqa: E402  (the stand-in Maya)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--core", required=True, help="the folder of the groomlab_core module for this Python")
    parser.add_argument("--out", required=True, help="the folder for the pictures")
    parser.add_argument("--dll-dir", action="append", default=[], help="a folder of DLLs the module needs (TBB)")
    parser.add_argument("--theme", action="append", default=[], help="a theme to picture (default: every one)")
    args = parser.parse_args()

    os.environ["QT_QPA_PLATFORM"] = "offscreen"
    # Qt offscreen reads fonts from a folder (no system font database): the system's, so text draws.
    fonts = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")
    if os.path.isdir(fonts):
        os.environ.setdefault("QT_QPA_FONTDIR", fonts)
    api = "PySide6" if sys.version_info >= (3, 11) else "PySide2"
    os.environ["GROOMLAB_QT_API"] = api
    os.environ["GROOMLAB_NO_UPDATES"] = "1"
    prefs = tempfile.mkdtemp(prefix="groomlab-theme-previews-")
    os.environ["GROOMLAB_UPDATES_DIR"] = prefs
    os.environ["GROOMLAB_USER_THEMES"] = os.path.join(prefs, "themes")
    for folder in args.dll_dir + [args.core]:
        if hasattr(os, "add_dll_directory") and os.path.isdir(folder):
            os.add_dll_directory(os.path.abspath(folder))
    sys.path[:0] = [qt_compat_check.SCRIPTS, os.path.abspath(args.core)]
    for name in [n for n in sys.modules if n == "maya" or n.startswith("maya.")]:
        del sys.modules[name]  # mayapy's own: the stand-in answers instead
    qt_compat_check._fake_maya(api, prefs)

    from groomlab.ui import qt
    QtWidgets = qt.QtWidgets
    application = QtWidgets.QApplication.instance() or QtWidgets.QApplication([])
    application.setFont(qt.QtGui.QFont("Segoe UI"))  # as Maya on Windows (else the folder's first font)
    from groomlab import graph as gm
    from groomlab.ui import graph_editor, palette, theme, widgets
    import groomlab_core

    def settle():
        for _ in range(4):
            application.processEvents()

    if not os.path.isdir(args.out):
        os.makedirs(args.out)
    catalog = gm.OperatorCatalog.from_json(groomlab_core.operator_specs_json())
    written = []
    for name in args.theme or theme.list_themes():
        theme.set_theme(name)

        tools = palette.ToolPalette()
        tools.resize(theme.px(420), theme.px(900))
        tools.show()
        settle()
        tools.work.setVisible(True)  # the tabs and tools, as with a groom (no groom here: the empty state)
        settle()
        path = os.path.join(args.out, "maya_groom_tools_%s.png" % name)
        tools.grab().save(path)
        written.append(path)
        menu = widgets.theme_menu(tools)
        menu.aboutToShow.emit()
        menu.adjustSize()
        path = os.path.join(args.out, "maya_theme_menu_%s.png" % name)
        menu.grab().save(path)
        written.append(path)
        tools.close()
        widgets.dispose(tools)

        editor = graph_editor.GraphEditor()
        editor.catalog = catalog
        editor.model = gm.GraphModel(catalog, groomlab_core.default_graph())
        editor.model.auto_layout()
        editor.rebuild()
        editor.resize(theme.px(1300), theme.px(700))
        editor.show()
        settle()
        # Close enough to read: the first few nodes (the sources to the look).
        near = [editor.items[n].sceneBoundingRect() for n in ("scalp", "guides", "scatter", "interpolate", "length", "noise")
                if n in editor.items]
        rect = near[0]
        for r in near[1:]:
            rect = rect.united(r)
        editor.view.frame_rect(rect)
        settle()
        selected = "interpolate" if "interpolate" in editor.items else sorted(editor.items)[0]
        editor.scene.clearSelection()
        editor.items[selected].setSelected(True)
        settle()
        path = os.path.join(args.out, "maya_graph_editor_%s.png" % name)
        editor.grab().save(path)
        written.append(path)
        editor.close()
        widgets.dispose(editor)
    for path in written:
        print("wrote %s" % path)
    return 0


if __name__ == "__main__":
    try:
        code = main()
    except Exception:
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    os._exit(code)  # no Qt teardown in mayapy

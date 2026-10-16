# SPDX-License-Identifier: Apache-2.0
"""GroomLab's Maya windows on small screens and with large text, headless: mayapy (its PySide2),
Qt offscreen and a stand-in Maya (qt_compat_check's), no window shown on the desktop.

Maya scales its UI itself (Preferences > Interface > UI Scaling, or the display's 125-150%): every
GroomLab size goes through theme.px(), the style sheet's fonts too. Each window is built at the
scales 1.0, 1.25 and 1.5 (theme.scale(), as Maya's dpiScale would answer) and must fit the screens
people run Maya on at that scale, in the physical pixels Maya 2022 lays out in: 1920x1080 at 125%
and 150%, 1366x768 at 100% (each less a taskbar). Its minimum size fits, its default size is
fitted within the screen, and the Graph Editor opens docked where a floating one would hide the
viewport.

    mayapy tests/hosts/maya/ui_scaling_check.py --core <folder of groomlab_core.pyd> [--dll-dir <dir>]...

Exit code 0 when all of it passed (ctest maya.ui_scaling).
"""

import argparse
import os
import sys
import tempfile
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import qt_compat_check  # noqa: E402  (the stand-in Maya)

# (scale, screen's usable area in Maya 2022's physical pixels, what it is)
SCREENS = (
    (1.0, (0, 0, 1366, 728), "1366x768 at 100%"),
    (1.0, (0, 0, 1920, 1040), "1920x1080 at 100%"),
    (1.25, (0, 0, 1920, 1030), "1920x1080 at 125%"),
    (1.5, (0, 0, 1920, 1020), "1920x1080 at 150%"),
)


def _expect(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--core", required=True, help="the folder of the groomlab_core module for this Python")
    parser.add_argument("--dll-dir", action="append", default=[], help="a folder of DLLs the module needs (TBB)")
    args = parser.parse_args()

    os.environ["QT_QPA_PLATFORM"] = "offscreen"
    api = "PySide6" if sys.version_info >= (3, 11) else "PySide2"
    os.environ["GROOMLAB_QT_API"] = api
    os.environ["GROOMLAB_NO_UPDATES"] = "1"
    prefs = tempfile.mkdtemp(prefix="groomlab-ui-scaling-")
    os.environ["GROOMLAB_UPDATES_DIR"] = prefs
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
    from groomlab import graph as gm
    from groomlab.ui import graph_editor, palette, theme, transfer_window, updates_window, widgets
    from groomlab.ui import reference_window, yeti_import_window
    import groomlab_core

    check = qt_compat_check.Check()

    # The pure parts: a size fitted to a screen, when the Graph Editor docks.
    def fitting():
        _expect(widgets.fit_size(1200, 600, (0, 0, 1920, 1040)) == (1200, 600), "room enough: as asked")
        w, h = widgets.fit_size(1800, 900, (0, 0, 1920, 1020))
        _expect(w <= 1920 * 0.9 and h <= 1020 * 0.9, "fitted: %r" % ((w, h),))
        _expect(widgets.fit_size(1800, 900, (0, 0, 400, 300)) == (400, 300), "a tiny screen: all of it")
        _expect(widgets.fit_size(700, 500, None) == (700, 500), "no screen: as asked")
        _expect(not graph_editor.open_docked((1200, 600), (0, 0, 1920, 1040)), "100% on 1920x1080 floats, as before")
        _expect(graph_editor.open_docked((1500, 750), (0, 0, 1920, 1030)), "125% on 1920x1080 docks")
        _expect(graph_editor.open_docked((1200, 600), (0, 0, 1366, 728)), "1366x768 docks")
    check.run("sizes fit the screen; the Graph Editor docks on small screens", fitting)

    def settle():
        for _ in range(3):
            application.processEvents()

    def fits(widget, screen, label):
        minimum = widget.minimumSizeHint().expandedTo(widget.minimumSize())
        _expect(minimum.width() <= screen[2] and minimum.height() <= screen[3],
                "%s: its minimum %dx%d is larger than the screen %dx%d" % (label, minimum.width(), minimum.height(),
                                                                           screen[2], screen[3]))
        default = widgets.fit_size(widget.width(), widget.height(), screen)
        _expect(default[0] <= screen[2] and default[1] <= screen[3], "%s: default %r" % (label, default))
        widget.resize(*default)
        settle()
        _expect(widget.width() <= screen[2] and widget.height() <= screen[3],
                "%s: %dx%d after fitting to %r" % (label, widget.width(), widget.height(), screen))
        return minimum

    catalog = gm.OperatorCatalog.from_json(groomlab_core.operator_specs_json())
    for scale, screen, about in SCREENS:
        theme._scale[0] = scale  # as Maya's dpiScale answers at this UI scaling

        def graph_editor_fits(scale=scale, screen=screen, about=about):
            editor = graph_editor.GraphEditor()
            editor.catalog = catalog
            editor.model = gm.GraphModel(catalog, groomlab_core.default_graph())
            editor.rebuild()
            editor.resize(theme.px(1200), theme.px(600))
            editor.show()
            settle()
            # The HairColor node's params (the color the user is after), in the param panel.
            if "color" in editor.items:
                editor.scene.clearSelection()
                editor.items["color"].setSelected(True)
                settle()
            minimum = fits(editor, screen, "Graph Editor")
            docked = graph_editor.open_docked((theme.px(1200), theme.px(600)), screen)
            print("      Graph Editor at %s: minimum %dx%d, %s" % (about, minimum.width(), minimum.height(),
                                                                  "docked" if docked else "floating"))
            editor.close()
            widgets.dispose(editor)
        check.run("Graph Editor fits %s" % about, graph_editor_fits)

        def palette_fits(screen=screen, about=about):
            tools = palette.ToolPalette()
            tools.resize(theme.px(420), screen[3])
            tools.show()
            settle()
            minimum = fits(tools, screen, "Groom Tools")
            # Docked beside the viewport: it must leave most of the screen's width to it.
            _expect(minimum.width() <= screen[2] * 0.35, "Groom Tools: %d wide at its narrowest" % minimum.width())
            tools.close()
            widgets.dispose(tools)
        check.run("Groom Tools fit %s" % about, palette_fits)

        def transfer_fits(screen=screen, about=about):
            window = transfer_window.TransferWindow(groom="groomLabShape1")
            window.show()
            settle()
            fits(window, screen, "Transfer window")
            window.close()
            widgets.dispose(window)
        check.run("Transfer window fits %s" % about, transfer_fits)

        def reference_fits(screen=screen, about=about):
            window = reference_window.ReferenceWindow(groom="groomLabShape1")
            window.show()
            settle()
            fits(window, screen, "Reference window")
            # The photo and the mesh views side by side at their minimum, the side panel beside them.
            _expect(window.photo_view.width() >= window.photo_view.minimumWidth(), "Reference window: the photo view")
            window.close()
            widgets.dispose(window)
        check.run("Reference window fits %s" % about, reference_fits)

        def yeti_import_fits(screen=screen, about=about):
            window = yeti_import_window.YetiImportWindow()
            window.show()
            settle()
            fits(window, screen, "Import from Yeti window")
            window.close()
            widgets.dispose(window)
        check.run("Import from Yeti window fits %s" % about, yeti_import_fits)

        def updates_fits(screen=screen, about=about):
            window = updates_window.UpdatesWindow()
            window.show()
            settle()
            fits(window, screen, "Updates window")
            window.close()
            widgets.dispose(window)
        check.run("Updates window fits %s" % about, updates_fits)
    theme._scale[0] = None

    print("%d passed, %d failed" % (check.passed, len(check.failures)))
    for label, trace in check.failures:
        print("FAILED: %s" % label)
    return 1 if check.failures else 0


if __name__ == "__main__":
    try:
        code = main()
    except Exception:
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    os._exit(code)  # no Qt teardown in mayapy

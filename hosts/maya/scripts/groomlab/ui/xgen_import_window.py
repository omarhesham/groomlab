# SPDX-License-Identifier: Apache-2.0
"""GroomLab > Import > From XGen...: the scene's XGen descriptions (both flavors) with checkboxes,
the Roots field (variables a texture's path needs that nothing sets, and folders to look in),
Import, and the report of what mapped, what was approximated, what was skipped and which files
were copied or not found. A plain window over Maya, never modal (Qt: groomlab.ui.qt). The work
is groomlab.import_xgen's.

    from groomlab.ui import xgen_import_window
    w = xgen_import_window.show()
    w.set_checked(["hairA"]); w.set_roots("ASSETS=D:/library"); w.import_checked()
"""

from groomlab.ui.qt import QtCore, QtWidgets

from groomlab.ui import theme, widgets

_window = None
ROOTS_VAR = "groomLabXGenImportRoots"  # the Roots field, remembered between sessions (optionVar)


class XGenImportWindow(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super(XGenImportWindow, self).__init__(parent)
        self.setWindowTitle("Import from XGen")
        self.setWindowIcon(widgets.icon("logo"))
        self.setObjectName("groomLabXGenImportWindow")
        outer = QtWidgets.QVBoxLayout(self)
        pad = theme.px(14)
        outer.setContentsMargins(pad, pad, pad, pad)
        outer.setSpacing(theme.px(8))

        title = QtWidgets.QLabel("Import from XGen")
        title.setProperty("role", "title")
        outer.addWidget(title)
        about = QtWidgets.QLabel("One GroomLab groom per description, on its scalp: guides, density, the modifiers as "
                                 "nodes, its maps copied into the scene's masks folder.")
        about.setProperty("role", "about")
        about.setWordWrap(True)
        outer.addWidget(about)

        self.list = QtWidgets.QListWidget()
        self.list.setObjectName("xgenDescriptions")
        self.list.setMinimumHeight(theme.px(90))
        outer.addWidget(self.list, 1)
        self.empty = QtWidgets.QLabel()
        self.empty.setProperty("role", "about")
        self.empty.setWordWrap(True)
        outer.addWidget(self.empty)

        roots_label = QtWidgets.QLabel("Roots (one per line: NAME=folder for a ${NAME} nothing sets, or a folder to "
                                       "look in)")
        roots_label.setWordWrap(True)
        outer.addWidget(roots_label)
        self.roots = QtWidgets.QPlainTextEdit()
        self.roots.setObjectName("xgenRoots")
        self.roots.setPlaceholderText("TEXTURE_ROOT=D:/library/textures\nC:/projects/show/sourceimages")
        self.roots.setMaximumHeight(theme.px(64))
        self.roots.setPlainText(_remembered_roots())
        outer.addWidget(self.roots)

        # The one button rule: secondary actions, the primary one on the right, Close last.
        buttons = QtWidgets.QHBoxLayout()
        buttons.addStretch(1)
        refresh = QtWidgets.QPushButton("Refresh")
        refresh.setToolTip("List the scene's XGen descriptions again")
        refresh.clicked.connect(self.refresh)
        buttons.addWidget(refresh)
        self.import_button = QtWidgets.QPushButton("Import")
        self.import_button.setProperty("role", "primary")
        self.import_button.setToolTip("Import the checked descriptions as GroomLab grooms")
        self.import_button.clicked.connect(self.import_checked)
        buttons.addWidget(self.import_button)
        close = QtWidgets.QPushButton("Close")
        close.clicked.connect(self.close)
        buttons.addWidget(close)
        outer.addLayout(buttons)

        self.report = QtWidgets.QPlainTextEdit()
        self.report.setObjectName("xgenReport")
        self.report.setReadOnly(True)
        self.report.setPlaceholderText("The import report shows here.")
        self.report.setMinimumHeight(theme.px(120))
        outer.addWidget(self.report, 2)
        theme.style(self)
        widgets.fit_to_screen(self, theme.px(560), theme.px(560))
        self.last_report = None
        self.refresh()

    def descriptions(self):
        """[(name, flavor, checked)] as the list shows them."""
        out = []
        for i in range(self.list.count()):
            item = self.list.item(i)
            out.append((item.data(QtCore.Qt.UserRole), item.data(QtCore.Qt.UserRole + 1),
                        item.checkState() == QtCore.Qt.Checked))
        return out

    def refresh(self):
        import groomlab
        checked = {name for name, _, on in self.descriptions() if on}
        first = self.list.count() == 0
        self.list.clear()
        try:
            found = groomlab.xgen_descriptions()
            problem = "" if found else "The scene has no XGen descriptions."
            if not found:
                from groomlab.importers import xgen
                if not xgen.xgen_available():
                    problem = ("XGen isn't loaded: load xgenToolkit in Windows > Settings/Preferences > Plug-in "
                               "Manager, then Refresh.")
        except Exception as e:  # XGen's plugin missing or broken
            from groomlab.ui import messages
            words = messages.plain(e)
            found, problem = [], words if words.startswith("XGen isn't loaded") else \
                "XGen isn't available (%s): load xgenToolkit in the Plug-in Manager, then Refresh" % words
        for d in found:
            flavor = "Interactive Groom" if d["flavor"] == "igs" else "Core (%s)" % d["palette"]
            item = QtWidgets.QListWidgetItem("%s    %s, on %s" % (d["name"], flavor, (d["scalp"] or "?").split("|")[-1]))
            item.setData(QtCore.Qt.UserRole, d["name"])
            item.setData(QtCore.Qt.UserRole + 1, d["flavor"])
            item.setFlags(item.flags() | QtCore.Qt.ItemIsUserCheckable)
            item.setCheckState(QtCore.Qt.Checked if first or d["name"] in checked else QtCore.Qt.Unchecked)
            self.list.addItem(item)
        self.empty.setText(problem)
        self.empty.setVisible(bool(problem))
        self.import_button.setEnabled(bool(found))

    def set_checked(self, names):
        for i in range(self.list.count()):
            item = self.list.item(i)
            item.setCheckState(QtCore.Qt.Checked if item.data(QtCore.Qt.UserRole) in names else QtCore.Qt.Unchecked)

    def set_roots(self, text):
        self.roots.setPlainText(text)

    def import_checked(self):
        """Import the checked descriptions; the report shows in the window (and the Script Editor).
        Returns the ImportReport, or None when nothing is checked."""
        import groomlab
        from maya import cmds
        names = [name for name, _, on in self.descriptions() if on]
        if not names:
            self.report.setPlainText("Check the descriptions to import.")
            return None
        roots = self.roots.toPlainText()
        cmds.optionVar(stringValue=(ROOTS_VAR, roots))
        self.import_button.setEnabled(False)
        QtWidgets.QApplication.setOverrideCursor(QtCore.Qt.WaitCursor)

        def progress(i, count, name):
            # Which one is being read, in the window and Maya's status line (the window repaints).
            text = "Importing %s %d of %d: %s..." % ("description", i + 1, count, name)
            self.report.setPlainText(text)
            self.report.repaint()
            QtWidgets.QApplication.processEvents(QtCore.QEventLoop.ExcludeUserInputEvents)
            print("GroomLab: " + text)

        try:
            report = groomlab.import_xgen(descriptions=names, roots=roots, progress=progress)
        except Exception as e:  # noqa: BLE001 (said in the window, the traceback in the Script Editor)
            from groomlab.ui import messages
            import traceback
            traceback.print_exc()
            self.report.setPlainText("The import failed: %s" % messages.plain(e))
            return None
        finally:
            QtWidgets.QApplication.restoreOverrideCursor()
            self.import_button.setEnabled(True)
        self.last_report = report
        text = report.text()
        unset = report.unset_variables
        if unset:
            text += "\n\nSome files were not found because nothing sets %s: add a line NAME=folder to Roots and " \
                    "import again." % ", ".join("${%s}" % v for v in unset)
        self.report.setPlainText(text)
        grooms = [g["groom"] for g in report.grooms if g["groom"]]
        if grooms:
            cmds.select([cmds.listRelatives(g, parent=True)[0] for g in grooms], replace=True)
        return report


def _remembered_roots():
    from maya import cmds
    return cmds.optionVar(query=ROOTS_VAR) if cmds.optionVar(exists=ROOTS_VAR) else ""


def show():
    """Open (or raise) the Import from XGen window, its list refreshed."""
    global _window
    from maya.app.general.mayaMixin import MayaQWidgetBaseMixin

    class MayaXGenImportWindow(MayaQWidgetBaseMixin, XGenImportWindow):
        pass

    if _window is not None and not widgets.alive(_window):
        _window = None
    if _window is None:
        _window = MayaXGenImportWindow()
        _window.setWindowFlags(_window.windowFlags() | QtCore.Qt.Window)
        widgets.remember_geometry(_window, "xgenImport")
        widgets.at_exit(_close_at_exit)
    else:
        _window.refresh()
    _window.show()
    _window.raise_()
    return _window


def window():
    """The open Import from XGen window, or None."""
    return _window if _window is not None and widgets.alive(_window) and _window.isVisible() else None


def _close_at_exit():
    global _window
    w, _window = _window, None
    if w is not None and widgets.alive(w):
        widgets.dispose(w)

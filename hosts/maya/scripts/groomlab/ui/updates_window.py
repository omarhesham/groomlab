# SPDX-License-Identifier: Apache-2.0
"""GroomLab > Help > Check for Updates...: this install's version, the newest release and its notes, Update
Now (download, verify, unpack beside this install; Maya's restart finishes it) or Later, and the
startup check's preference (Qt: groomlab.ui.qt). A plain window over Maya, never modal: a scripted session that
opens it goes on. The work is groomlab.updates'.

    from groomlab.ui import updates_window
    updates_window.show()

UpdateNote: the Groom Tools' quiet one-line note when the startup check found a newer release.
"""

import threading

from groomlab.ui.qt import QtCore, QtWidgets

from groomlab import updates
from groomlab.ui import theme, widgets

_window = None


class UpdatesWindow(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super(UpdatesWindow, self).__init__(parent)
        self.setWindowTitle("GroomLab Updates")
        self.setWindowIcon(widgets.icon("logo"))
        self.setObjectName("groomLabUpdatesWindow")
        outer = QtWidgets.QVBoxLayout(self)
        pad = theme.px(14)
        outer.setContentsMargins(pad, pad, pad, pad)
        outer.setSpacing(theme.px(8))

        self.title = QtWidgets.QLabel("GroomLab Updates")
        self.title.setProperty("role", "title")
        outer.addWidget(self.title)
        self.installed = QtWidgets.QLabel()
        self.installed.setProperty("role", "about")
        self.installed.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)
        outer.addWidget(self.installed)
        self.status = QtWidgets.QLabel()
        self.status.setObjectName("updatesStatus")
        self.status.setWordWrap(True)
        outer.addWidget(self.status)
        self.notes = QtWidgets.QLabel()
        self.notes.setObjectName("updatesNotes")
        self.notes.setWordWrap(True)
        self.notes.setProperty("role", "about")
        self.notes.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)
        self.notes.setVisible(False)
        outer.addWidget(self.notes)
        self.progress = QtWidgets.QProgressBar()
        self.progress.setVisible(False)
        outer.addWidget(self.progress)

        # The one button rule: secondary actions, the primary one on the right, Close (Later) last.
        buttons = QtWidgets.QHBoxLayout()
        buttons.addStretch(1)
        self.check_button = QtWidgets.QPushButton("Check Again")
        self.check_button.clicked.connect(self.check)
        buttons.addWidget(self.check_button)
        self.update_button = QtWidgets.QPushButton("Update Now")
        self.update_button.setProperty("role", "primary")
        self.update_button.setToolTip("Download the new version, check it and install it beside this one; restart Maya "
                                      "to finish (this version's folder goes once the new one has started)")
        self.update_button.clicked.connect(self.update_now)
        buttons.addWidget(self.update_button)
        self.later_button = QtWidgets.QPushButton("Later")
        self.later_button.setToolTip("Close; GroomLab > Help > Check for Updates... any time")
        self.later_button.clicked.connect(self.close)
        buttons.addWidget(self.later_button)
        outer.addLayout(buttons)

        self.startup = QtWidgets.QCheckBox("Check for updates when Maya starts (once a day)")
        self.startup.setToolTip("A quiet note in the Groom Tools when a new version is out; never a dialog. Studios: "
                                "turn it off here, or set GROOMLAB_NO_UPDATES=1 for every machine")
        self.startup.setChecked(updates.startup_check_enabled())
        self.startup.toggled.connect(updates.set_startup_check)
        outer.addWidget(self.startup)
        outer.addStretch(1)
        theme.style(self)
        widgets.fit_to_screen(self, theme.px(520), theme.px(240))

        self.result = None
        self._job = None   # {"done", "total", "result"} of an install running on its thread
        self._checking = False
        self._timer = QtCore.QTimer(self)
        self._timer.timeout.connect(self._poll)
        self._show(None)

    def busy(self):
        """A check or an install is running."""
        return self._checking or self._job is not None

    def _show(self, result):
        """The window for a check's result (None: before any)."""
        self.result = result
        self.installed.setText("Installed: GroomLab %s" % (updates.current_version() or "(unknown version)"))
        off = updates.off_reason()
        if off:
            self.status.setText(off)
        elif result is None:
            self.status.setText("Checking for updates..." if self._checking else "")
        else:
            self.status.setText(result["message"])
        notes = (result or {}).get("notes") or ""
        if result and result.get("newer") and notes:
            self.notes.setText("What's new in %s:\n%s" % (result["latest"], notes))
            self.notes.setVisible(True)
        else:
            self.notes.setVisible(False)
        installable = bool(result and result.get("installable"))
        self.update_button.setVisible(installable or self._job is not None)
        self.update_button.setEnabled(installable and not self.busy())
        self.check_button.setEnabled(not off and not self.busy())
        self.startup.setEnabled(not off)

    def check(self):
        """Asks for the newest release on a thread; the window shows the answer when it comes."""
        if self.busy():
            return
        if updates.off_reason():
            self._show(None)
            return
        self._checking = True
        self._show(None)
        updates.check_in_background(done=self._checked)

    def _checked(self, result):
        self._checking = False
        if not widgets.alive(self):
            return
        if result.get("installable"):
            updates._announce(result)  # the Groom Tools' note too
        self._show(result)

    def update_now(self):
        """Installs the release found on a thread, the progress bar following the download."""
        if self.busy() or not (self.result and self.result.get("installable")):
            return
        info = self.result["info"]
        job = {"done": 0, "total": int(info.get("size") or 0), "result": None}

        def progress(done, total):
            job["done"], job["total"] = done, total

        def work():
            job["result"] = updates.install(info, progress=progress)

        self._job = job
        self.progress.setRange(0, 1000)
        self.progress.setValue(0)
        self.progress.setVisible(True)
        self.status.setText("Downloading GroomLab %s..." % self.result["latest"])
        self.update_button.setEnabled(False)
        self.check_button.setEnabled(False)
        thread = threading.Thread(target=work, name="GroomLabUpdateInstall")
        thread.daemon = True
        thread.start()
        self._timer.start(100)

    def _poll(self):
        job = self._job
        if job is None:
            self._timer.stop()
            return
        if job["total"]:
            self.progress.setValue(int(1000 * min(job["done"], job["total"]) / float(job["total"])))
        result = job["result"]
        if result is None:
            return
        self._timer.stop()
        self._job = None
        self.progress.setVisible(False)
        if result.get("ok"):
            updates._announce(None)  # nothing more to offer this session
            self.status.setText(result["message"])
            self.notes.setVisible(False)
            self.update_button.setVisible(False)
            self.check_button.setEnabled(False)
            self.later_button.setText("Close")
        else:
            self.status.setText("The update was not installed: %s" % result["message"])
            self.update_button.setEnabled(True)
            self.check_button.setEnabled(True)
        self.install_result = result

    def closeEvent(self, event):
        self._timer.stop()
        super(UpdatesWindow, self).closeEvent(event)


def show(check=True):
    """Open (or raise) the Updates window; `check` asks for the newest release at once."""
    global _window
    from maya.app.general.mayaMixin import MayaQWidgetBaseMixin

    class MayaUpdatesWindow(MayaQWidgetBaseMixin, UpdatesWindow):
        pass

    if _window is not None and not widgets.alive(_window):
        _window = None
    if _window is None:
        _window = MayaUpdatesWindow()
        _window.setWindowFlags(_window.windowFlags() | QtCore.Qt.Window)
        widgets.remember_geometry(_window, "updates")
        widgets.at_exit(_close_at_exit)
    _window.show()
    _window.raise_()
    if _window._job is not None:
        _window._timer.start(100)  # an install still running (the window was closed meanwhile)
    elif check and not _window.busy():
        found = updates.available()
        if found:
            _window._show(found)  # the startup check's answer, without asking again
        else:
            _window.check()
    return _window


def window():
    """The open Updates window, or None."""
    return _window if _window is not None and widgets.alive(_window) and _window.isVisible() else None


def _close_at_exit():
    global _window
    w, _window = _window, None
    if w is not None and widgets.alive(w):
        w._timer.stop()
        widgets.dispose(w)


class UpdateNote(QtWidgets.QLabel):
    """"GroomLab X is available - GroomLab > Check for Updates" under a window's header when the
    startup check found a newer release; hidden otherwise. The link opens the Updates window."""

    def __init__(self, parent=None):
        super(UpdateNote, self).__init__(parent)
        self.setObjectName("groomLabUpdateNote")
        self.setWordWrap(True)
        self.setProperty("role", "about")
        self.setTextFormat(QtCore.Qt.RichText)
        self.linkActivated.connect(lambda _: show())
        updates.on_available(self._changed)
        self.refresh()

    def _changed(self, result):
        if widgets.alive(self):
            self.refresh(result)
        else:
            updates.remove_listener(self._changed)

    def refresh(self, result=None):
        result = result or updates.available()
        if not result:
            self.setText("")
            self.setVisible(False)
            return
        self.setText('GroomLab %s is available \u2014 <a href="updates">GroomLab &gt; Help &gt; Check for Updates</a>'
                     % result["latest"])
        self.setToolTip("A new GroomLab version is out: see what's new and update in GroomLab > Help > Check for Updates...")
        self.setVisible(True)

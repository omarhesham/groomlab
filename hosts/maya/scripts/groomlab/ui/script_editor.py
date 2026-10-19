# SPDX-License-Identifier: Apache-2.0
"""The script node editor (Graph Editor: Edit Node... on a script node; New Script Node in Add Node).

It opens in the Graph Editor's side panel, never as a dialog: the node's header as a small form
(label, category, description, author, id, version, what the script writes), its params as a table
(add, remove, edit name, type, default, min, max, mappable, label, choices), the script in a code
editor (line numbers, highlighting, completion), and what is wrong, each error by line and column
with its line tinted. It is live: a change is checked after a short pause (300 ms) and, when it
checks, the node takes it and the groom evaluates. Save to Library writes the node's file (one
version up); Save as New writes it under a new id.

    ScriptEdit        the code editor: expression_editor.CodeEdit with line numbers
    ParamsTable       the header's params
    ScriptNodePanel   all of it, for one Script node of the Graph Editor
"""

import copy
import json

from groomlab.ui.qt import QtCore, QtGui, QtWidgets

from groomlab import expressions
from groomlab.ui import theme
from groomlab.ui.expression_editor import CodeEdit

PARAM_TYPES = ["number", "length", "int", "bool", "enum", "color", "ramp"]
COLUMNS = ["Name", "Type", "Default", "Min", "Max", "Mappable", "Label", "Choices"]
WRITES = ["P", "width", "color"]

# What completes in a script beyond an expression's names and functions.
SCRIPT_WORDS = [
    ("per strand {", "A block run once per strand, before its points"),
    ("per point {", "A block run for every point of every strand, root to tip"),
    ("for $i in 0 .. 8 {", "A loop: $i from 0 to 7 (bounds: whole numbers, or a whole-number param with a max)"),
    ("P", "The point (per point; the root point per strand). Writable when \"writes\" lists P"),
    ("width", "The point's width (per strand: every point's). Writable when \"writes\" lists width"),
    ("color", "The point's color [r, g, b] (per strand: every point's). Writable when \"writes\" lists color"),
    ("channel.", "channel.name: a channel, per strand or per point; writable when \"writes\" lists it"),
    ("tangent", "The strand's direction at the point, as it came in"),
    ("prev.P", "The point before, as this run left it (the root's own at the root)"),
    ("root", "The strand's first point"),
    ("tip", "The strand's last point"),
    ("pointCount", "The strand's points"),
    ("pointP", "The point as it came in"),
]


def script_completions(params=()):
    """(text, what it is) for the code editor: the header's params as $name, the script's own
    words and functions, then the expression language's."""
    import groomlab_core
    out = []
    for p in params:
        name = p.get("name", "")
        if not name:
            continue
        if p.get("type") == "ramp":
            out.append(("$%s(" % name, "Your ramp param: $%s(x), x from 0 to 1" % name))
        else:
            out.append(("$" + name, "Your param (%s)" % p.get("type", "number")))
    out += SCRIPT_WORDS
    out += [("%s(" % f["name"], "%s(%s): %s" % (f["name"], f["arguments"], f["about"])) for f in groomlab_core.script_functions()]
    return out + expressions.completions()


class LineNumbers(QtWidgets.QWidget):
    """The code editor's gutter."""

    def __init__(self, edit):
        super(LineNumbers, self).__init__(edit)
        self.edit = edit

    def sizeHint(self):
        return QtCore.QSize(self.edit.gutter_width(), 0)

    def paintEvent(self, event):
        self.edit.paint_gutter(event)


class ScriptEdit(CodeEdit):
    """A script: CodeEdit (completion, highlighting, a commit after a pause) with line numbers, the
    lines with errors marked in the gutter and tinted."""

    def __init__(self, text="", parent=None):
        super(ScriptEdit, self).__init__(text, lines=14, parent=parent)
        self.setLineWrapMode(QtWidgets.QPlainTextEdit.NoWrap)  # code: lines as written
        self.pause.setInterval(300)
        self.error_lines = set()
        self.gutter = LineNumbers(self)
        self.blockCountChanged.connect(self._fit_gutter)
        self.updateRequest.connect(self._update_gutter)
        self._fit_gutter()

    def gutter_width(self):
        digits = len(str(max(1, self.blockCount())))
        return theme.px(10) + self.fontMetrics().horizontalAdvance("9") * max(2, digits)

    def _fit_gutter(self, *_):
        self.setViewportMargins(self.gutter_width(), 0, 0, 0)

    def _update_gutter(self, rect, dy):
        if dy:
            self.gutter.scroll(0, dy)
        else:
            self.gutter.update(0, rect.y(), self.gutter.width(), rect.height())
        if rect.contains(self.viewport().rect()):
            self._fit_gutter()

    def resizeEvent(self, event):
        super(ScriptEdit, self).resizeEvent(event)
        r = self.contentsRect()
        self.gutter.setGeometry(QtCore.QRect(r.left(), r.top(), self.gutter_width(), r.height()))

    def paint_gutter(self, event):
        t = theme.current()
        painter = QtGui.QPainter(self.gutter)
        painter.fillRect(event.rect(), t.qcolor("ui.panel") if hasattr(t, "qcolor") else QtGui.QColor(40, 42, 46))
        block = self.firstVisibleBlock()
        number = block.blockNumber()
        top = int(self.blockBoundingGeometry(block).translated(self.contentOffset()).top())
        bottom = top + int(self.blockBoundingRect(block).height())
        dim = t.qcolor("ui.textDim")
        error = t.qcolor(t.get("ui.warning", "yellow"))
        while block.isValid() and top <= event.rect().bottom():
            if block.isVisible() and bottom >= event.rect().top():
                painter.setPen(error if number + 1 in self.error_lines else dim)
                painter.drawText(0, top, self.gutter.width() - theme.px(5), self.fontMetrics().height(),
                                 QtCore.Qt.AlignRight, str(number + 1))
            block = block.next()
            top = bottom
            bottom = top + int(self.blockBoundingRect(block).height())
            number += 1

    def mark_errors(self, errors):
        """Tint each script error's line (errors: script_node_errors' entries)."""
        self.error_lines = set(e["line"] for e in errors if e.get("part") == "script" and e.get("line"))
        selections = []
        tint = theme.current().qcolor(theme.current().get("ui.warning", "yellow"), 60)
        for line in sorted(self.error_lines):
            block = self.document().findBlockByNumber(line - 1)
            if not block.isValid():
                continue
            selection = QtWidgets.QTextEdit.ExtraSelection()
            selection.format.setBackground(tint)
            selection.format.setProperty(QtGui.QTextFormat.FullWidthSelection, True)
            selection.cursor = QtGui.QTextCursor(block)
            selections.append(selection)
        self.setExtraSelections(selections)
        self.gutter.update()

    def mark(self, message):  # CodeEdit's own (an expression's message): the panel marks instead
        pass

    def go_to(self, line, column=1):
        block = self.document().findBlockByNumber(max(line, 1) - 1)
        if block.isValid():
            cursor = QtGui.QTextCursor(block)
            cursor.movePosition(QtGui.QTextCursor.Right, QtGui.QTextCursor.MoveAnchor,
                                min(max(column - 1, 0), max(block.length() - 1, 0)))
            self.setTextCursor(cursor)
            self.setFocus()


def _number_text(value):
    if value is None:
        return ""
    if isinstance(value, float) and value == int(value) and abs(value) < 1e15:
        return str(int(value))
    return str(value)


def default_text(param):
    """A param's default as its table cell shows it."""
    value = param.get("default")
    kind = param.get("type", "number")
    if value is None:
        return ""
    if kind == "bool":
        return "true" if value else "false"
    if kind == "color" and isinstance(value, list):
        return ", ".join(_number_text(v) for v in value)
    if kind == "ramp":
        return json.dumps(value)
    if isinstance(value, (int, float)):
        return _number_text(value)
    return str(value)


def default_value(text, kind):
    """A table cell's text as the param's default (text the core then says is wrong when it is;
    None for empty: the type's own default)."""
    text = (text or "").strip()
    if not text:
        return None
    try:
        if kind in ("number", "length"):
            return float(text)
        if kind == "int":
            return int(round(float(text)))
        if kind == "bool":
            return text.lower() in ("1", "true", "yes", "on")
        if kind == "color":
            values = [float(v) for v in text.replace("[", "").replace("]", "").split(",")]
            return values if len(values) == 3 else text
        if kind == "ramp":
            return json.loads(text)
    except ValueError:
        return text
    return text


class ParamsTable(QtWidgets.QTableWidget):
    """The header's params, a row each (Name, Type, Default, Min, Max, Mappable, Label, Choices).
    Fields the table does not show (description, unit, softMin, softMax) are kept by name.
    Signal edited() on any change."""

    edited = QtCore.Signal()

    def __init__(self, params=(), parent=None):
        super(ParamsTable, self).__init__(0, len(COLUMNS), parent)
        self.setObjectName("scriptNodeParams")
        self.setHorizontalHeaderLabels(COLUMNS)
        self.verticalHeader().setVisible(False)
        self.horizontalHeader().setStretchLastSection(True)
        self.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
        self.kept = {}  # name -> the param's entry as it came (its fields the table does not show)
        self._filling = False
        self.itemChanged.connect(lambda _item: self._changed())
        self.set_params(params)

    def _changed(self):
        if not self._filling:
            self.edited.emit()

    def set_params(self, params):
        self._filling = True
        try:
            self.setRowCount(0)
            self.kept = dict((p.get("name"), copy.deepcopy(p)) for p in params)
            for p in params:
                self.add_row(p)
        finally:
            self._filling = False
        self.setMinimumHeight(theme.px(26) * min(max(len(params), 2), 6) + theme.px(30))

    def add_row(self, param=None):
        param = param or {"name": self.free_name(), "type": "number", "default": 0}
        row = self.rowCount()
        filling = self._filling
        self._filling = True
        try:
            self.insertRow(row)
            self.setItem(row, 0, QtWidgets.QTableWidgetItem(param.get("name", "")))
            kind = QtWidgets.QComboBox()
            kind.addItems(PARAM_TYPES)
            kind.setCurrentText(param.get("type", "number"))
            kind.currentTextChanged.connect(lambda _t: self._changed())
            self.setCellWidget(row, 1, kind)
            self.setItem(row, 2, QtWidgets.QTableWidgetItem(default_text(param)))
            self.setItem(row, 3, QtWidgets.QTableWidgetItem(_number_text(param.get("min"))))
            self.setItem(row, 4, QtWidgets.QTableWidgetItem(_number_text(param.get("max"))))
            mappable = QtWidgets.QCheckBox()
            mappable.setChecked(bool(param.get("mappable")))
            mappable.setToolTip("A map, an expression or a field drives it per strand or per point (number and length)")
            mappable.toggled.connect(lambda _on: self._changed())
            self.setCellWidget(row, 5, mappable)
            self.setItem(row, 6, QtWidgets.QTableWidgetItem(param.get("label", "")))
            self.setItem(row, 7, QtWidgets.QTableWidgetItem(", ".join(param.get("choices") or [])))
        finally:
            self._filling = filling
        self._changed()
        return row

    def free_name(self):
        taken = set(self.item(r, 0).text() for r in range(self.rowCount()) if self.item(r, 0))
        i = 1
        while "param%d" % i in taken:
            i += 1
        return "param%d" % i

    def remove_selected(self):
        rows = sorted(set(i.row() for i in self.selectedIndexes()), reverse=True)
        if not rows and self.rowCount():
            rows = [self.rowCount() - 1]
        for row in rows:
            self.removeRow(row)
        if rows:
            self._changed()

    def text(self, row, column):
        item = self.item(row, column)
        return item.text().strip() if item else ""

    def params(self):
        """The rows as header params (the fields kept by name added back)."""
        out = []
        for row in range(self.rowCount()):
            name = self.text(row, 0)
            kind = self.cellWidget(row, 1).currentText()
            p = dict((k, v) for k, v in (self.kept.get(name) or {}).items()
                     if k in ("description", "unit", "softMin", "softMax"))
            p["name"] = name
            p["type"] = kind
            if kind != "number":
                p.pop("unit", None)
            value = default_value(self.text(row, 2), kind)
            if value is not None:
                p["default"] = value
            for column, key in ((3, "min"), (4, "max")):
                text = self.text(row, column)
                if text:
                    try:
                        p[key] = float(text)
                    except ValueError:
                        p[key] = text  # the core says what is wrong
            if kind not in ("number", "length", "int"):
                for key in ("min", "max", "softMin", "softMax"):
                    p.pop(key, None)
            if self.cellWidget(row, 5).isChecked():
                p["mappable"] = True
            if self.text(row, 6):
                p["label"] = self.text(row, 6)
            if kind == "enum":
                p["choices"] = [c.strip() for c in self.text(row, 7).split(",") if c.strip()]
            out.append(p)
        return out


class ScriptNodePanel(QtWidgets.QWidget):
    """A Script node's editor in the Graph Editor's side panel (editor: the GraphEditor). Its widgets
    are in `fields` (tests drive them): label, category, description, author, id, version,
    writes (P, width, color checkboxes), channels, params, add, remove, script, errors, status,
    save, save_new, done."""

    def __init__(self, editor, node):
        super(ScriptNodePanel, self).__init__()
        self.setObjectName("scriptNodeEditor")
        self.editor, self.node = editor, node
        definition = copy.deepcopy(editor.model.param(node, "definition") or {})
        self.base = definition  # fields the form does not show (inputs, output, size, license...)
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(theme.px(10), theme.px(4), theme.px(10), theme.px(10))
        layout.setSpacing(theme.px(6))

        bar = QtWidgets.QHBoxLayout()
        title = QtWidgets.QLabel("Script node: %s" % node)
        title.setProperty("role", "title")
        bar.addWidget(title, 1)
        done = QtWidgets.QPushButton("Done")
        done.setObjectName("scriptNodeDone")
        done.setToolTip("Back to the node's params")
        bar.addWidget(done)
        layout.addLayout(bar)
        about = QtWidgets.QLabel("Live: each change is checked after a pause and the node takes it when it checks. "
                                 "Save to Library to use it in other grooms, the standalone app and Blender.")
        about.setWordWrap(True)
        about.setProperty("role", "about")
        layout.addWidget(about)

        form = QtWidgets.QFormLayout()
        form.setLabelAlignment(QtCore.Qt.AlignRight)
        label = QtWidgets.QLineEdit(definition.get("label", ""))
        category = QtWidgets.QComboBox()
        category.setEditable(True)
        try:
            from groomlab.ui.graph_editor import CATEGORY_COLORS
            titles = [c for c in CATEGORY_COLORS if c not in ("Output", "Compound")]
        except ImportError:
            titles = []
        category.addItems(["My Nodes"] + titles)
        category.setCurrentText(definition.get("category") or "My Nodes")
        description = QtWidgets.QLineEdit(definition.get("description", ""))
        author = QtWidgets.QLineEdit(definition.get("author", ""))
        node_id = QtWidgets.QLineEdit(definition.get("id", ""))
        node_id.setToolTip("Never changes once shared: a newer version keeps its id (yourname.node_name)")
        version = QtWidgets.QSpinBox()
        version.setRange(1, 1000000)
        version.setValue(int(definition.get("version", 1) or 1))
        writes_row = QtWidgets.QHBoxLayout()
        written = list(definition.get("writes") or [])
        writes = {}
        for what in WRITES:
            box = QtWidgets.QCheckBox(what)
            box.setChecked(what in written)
            box.setToolTip("The script may assign %s" % what)
            writes[what] = box
            writes_row.addWidget(box)
        channels = QtWidgets.QLineEdit(", ".join(w[len("channel."):] for w in written if w.startswith("channel.")))
        channels.setPlaceholderText("channels it writes, e.g. mask, kinked")
        writes_row.addWidget(channels, 1)
        for name, widget in (("Label", label), ("Category", category), ("Description", description), ("Author", author),
                             ("Id", node_id), ("Version", version)):
            form.addRow(name, widget)
        form.addRow("Writes", writes_row)
        layout.addLayout(form)

        heading = QtWidgets.QLabel("PARAMS")
        heading.setProperty("role", "subheading")
        layout.addWidget(heading)
        params = ParamsTable(definition.get("params") or [])
        layout.addWidget(params)
        rows = QtWidgets.QHBoxLayout()
        add = QtWidgets.QPushButton("Add Param")
        add.setObjectName("scriptNodeAddParam")
        remove = QtWidgets.QPushButton("Remove")
        remove.setObjectName("scriptNodeRemoveParam")
        remove.setToolTip("Remove the selected params")
        rows.addWidget(add)
        rows.addWidget(remove)
        rows.addStretch(1)
        layout.addLayout(rows)

        heading = QtWidgets.QLabel("SCRIPT")
        heading.setProperty("role", "subheading")
        layout.addWidget(heading)
        script = ScriptEdit(definition.get("script", ""))
        script.setObjectName("scriptNodeCode")
        layout.addWidget(script, 1)
        hint = QtWidgets.QLabel("per strand { ... } then per point { ... }; P, width, color and channel.name as "
                                "\"writes\" lists them; $param; for $i in 0 .. 8 { }. Ctrl+Space completes.")
        hint.setWordWrap(True)
        hint.setProperty("role", "hint")
        layout.addWidget(hint)
        errors = QtWidgets.QListWidget()
        errors.setObjectName("scriptNodeErrors")
        errors.setMaximumHeight(theme.px(90))
        errors.setVisible(False)
        layout.addWidget(errors)
        status = QtWidgets.QLabel()
        status.setObjectName("scriptNodeStatus")
        status.setWordWrap(True)
        status.setProperty("role", "status")
        layout.addWidget(status)
        buttons = QtWidgets.QHBoxLayout()
        save = QtWidgets.QPushButton("Save to Library")
        save.setObjectName("scriptNodeSave")
        save.setToolTip("Write this node to your node library (its file, one version up when the library has it)")
        save_new = QtWidgets.QPushButton("Save as New")
        save_new.setObjectName("scriptNodeSaveNew")
        save_new.setToolTip("Write it as a new node of the library, under the Id above (one the library does not have)")
        buttons.addStretch(1)
        buttons.addWidget(save_new)
        buttons.addWidget(save)
        layout.addLayout(buttons)

        self.fields = {"label": label, "category": category, "description": description, "author": author,
                       "id": node_id, "version": version, "writes": writes, "channels": channels, "params": params,
                       "add": add, "remove": remove, "script": script, "errors": errors, "status": status,
                       "save": save, "save_new": save_new, "done": done}
        # Live: the header and table after a pause; the script after its own (CodeEdit's commit).
        self.timer = QtCore.QTimer(self)
        self.timer.setSingleShot(True)
        self.timer.setInterval(300)
        self.timer.timeout.connect(self.check)
        for widget in (label, description, author, node_id, channels):
            widget.textEdited.connect(lambda _t: self.timer.start())
        category.currentTextChanged.connect(lambda _t: self.timer.start())
        version.valueChanged.connect(lambda _v: self.timer.start())
        for box in writes.values():
            box.toggled.connect(lambda _on: self.timer.start())
        params.edited.connect(self.timer.start)
        params.edited.connect(self.update_completions)
        script.committed.connect(lambda _t: self.check())
        add.clicked.connect(lambda _=False: params.add_row())
        remove.clicked.connect(lambda _=False: params.remove_selected())
        errors.itemClicked.connect(self.jump)
        save.clicked.connect(lambda _=False: self.save())
        save_new.clicked.connect(lambda _=False: self.save(as_new=True))
        done.clicked.connect(lambda _=False: self.editor.close_script_editor(self.node))
        self.update_completions()
        self.show_errors(self.errors())

    # -- The node file the form makes ------------------------------------------------------

    def definition(self):
        """The node's file as the form has it (fields it does not show kept)."""
        f = self.fields
        data = copy.deepcopy(self.base)
        data["glnode"] = data.get("glnode", 1)
        data["id"] = f["id"].text().strip()
        data["version"] = f["version"].value()
        data["label"] = f["label"].text().strip()
        category = f["category"].currentText().strip()
        data["category"] = "" if category == "My Nodes" else category
        data["description"] = f["description"].text().strip()
        author = f["author"].text().strip()
        if author:
            data["author"] = author
        else:
            data.pop("author", None)
        data["params"] = f["params"].params()
        writes = [w for w in WRITES if f["writes"][w].isChecked()]
        writes += ["channel." + c.strip() for c in f["channels"].text().split(",") if c.strip()]
        data["writes"] = writes
        data["script"] = f["script"].toPlainText()
        return data

    def errors(self, definition=None):
        import groomlab_core
        return list(groomlab_core.script_node_errors(json.dumps(definition or self.definition())))

    def show_errors(self, errors):
        listing = self.fields["errors"]
        listing.clear()
        for e in errors:
            where = ("line %d, column %d: " % (e["line"], e["column"])) if e.get("line") else (e["part"] + ": ")
            item = QtWidgets.QListWidgetItem(where + e["message"])
            item.setData(QtCore.Qt.UserRole, (e.get("line", 0), e.get("column", 0)))
            item.setForeground(theme.current().qcolor(theme.current().get("ui.warning", "yellow")))
            listing.addItem(item)
        listing.setVisible(bool(errors))
        self.fields["script"].mark_errors(errors)

    def jump(self, item):
        line, column = item.data(QtCore.Qt.UserRole) or (0, 0)
        if line:
            self.fields["script"].go_to(line, column)

    def update_completions(self):
        params = self.fields["params"].params()
        self.fields["script"].set_entries(script_completions(params), [p["name"] for p in params])

    # -- Live ----------------------------------------------------------------------------------

    def check(self):
        """The form checked now: its errors listed, and the node takes it when it checks. True then."""
        self.timer.stop()
        definition = self.definition()
        errors = self.errors(definition)
        self.show_errors(errors)
        if errors:
            self.fields["status"].setText("Not applied: the node keeps its last script that checked.")
            return False
        if definition != self.editor.model.param(self.node, "definition"):
            self.editor.set_script_definition(self.node, definition)
            self.base = copy.deepcopy(definition)
        # What the evaluation said of the node (past its step budget, say), else that it took it.
        error = (getattr(self.editor, "node_errors", None) or {}).get(self.node)
        note = self.editor.note_of(self.node) if hasattr(self.editor, "note_of") else ""
        self.fields["status"].setText("Error: %s" % error if error else (note or "Applied: the node runs this script."))
        return True

    def save(self, as_new=False):
        """Save to Library (as_new: as a new node, under an id the library does not have). The node
        takes the saved copy. Returns the file's path, None (said in the errors) when it cannot."""
        import groomlab_core
        if not self.check():
            return None
        definition = self.definition()
        listed = dict((n["id"], n) for n in groomlab_core.library_nodes()["nodes"])
        node_id = definition["id"]
        if as_new:
            if node_id in listed:
                self.show_errors([{"part": "header", "message": "the library has a node %s already: give the new node "
                                   "another Id" % node_id, "line": 0, "column": 0}])
                return None
            definition["version"] = 1
        elif node_id in listed:
            definition["version"] = max(definition["version"], listed[node_id]["version"] + 1)
        path = self.editor.save_script_node(self.node, definition)
        if path:
            self.base = copy.deepcopy(definition)
            self.fields["version"].blockSignals(True)
            self.fields["version"].setValue(definition["version"])
            self.fields["version"].blockSignals(False)
            self.fields["status"].setText("Saved %s (version %d) to %s" % (definition["label"], definition["version"], path))
        return path

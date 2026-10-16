# SPDX-License-Identifier: Apache-2.0
"""Runs editor_cases.py (the Graph Editor and Groom Tools widget tests, unittest) in the GUI
session: they need a QApplication. A suite of gui_runner.py (ctest maya.gui; harness:
gui_support.py); the unittest report goes to the session's log, each failure named in its summary.

GROOMLAB_GUI_CASES (comma-separated parts of test names: ctrl_z,set_key) runs only those tests.
"""

import os
import unittest

from gui_support import LogStream, fail, finish, soon


def start():
    soon(run)  # not in an evalDeferred: that would make the whole run one open undo chunk


def _only(suite, parts):
    for test in suite:
        if isinstance(test, unittest.TestSuite):
            for inner in _only(test, parts):
                yield inner
        elif any(part in test.id() for part in parts):
            yield test


def run():
    import editor_cases
    stream = LogStream()
    suite = unittest.defaultTestLoader.loadTestsFromModule(editor_cases)
    parts = [p.strip() for p in os.environ.get("GROOMLAB_GUI_CASES", "").split(",") if p.strip()]
    if parts:
        suite = unittest.TestSuite(list(_only(suite, parts)))
    result = unittest.TextTestRunner(stream=stream, verbosity=2).run(suite)
    stream.close()
    for test, _ in result.failures:
        fail("FAIL " + test.id())
    for test, _ in result.errors:
        fail("ERROR " + test.id())
    for test in result.unexpectedSuccesses:
        fail("unexpected success " + test.id())
    finish()

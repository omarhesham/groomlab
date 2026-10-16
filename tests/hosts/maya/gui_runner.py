# SPDX-License-Identifier: Apache-2.0
"""The GUI test session (ctest maya.gui): one Maya window runs every GUI suite in turn.

Exec'd by the generated gui.mel (maya.exe -script gui.mel). Reads GROOMLAB_PLUGIN,
GROOMLAB_SCRIPTS and GROOMLAB_TESTS (this folder) from the environment, writes the log to
GROOMLAB_GUI_LOG and quits Maya with exit code 0 when every suite passed. GROOMLAB_GUI_SUITES
(brush,viewport,...) runs only some suites. The suites and the harness: gui_support.py.
"""

import os
import sys
import traceback

import maya.mel
import maya.utils
from maya import cmds


def _load():
    scripts = os.environ["GROOMLAB_SCRIPTS"]
    for path in (scripts, os.environ["GROOMLAB_TESTS"]):
        if path not in sys.path:
            sys.path.insert(0, path)
    try:
        cmds.loadPlugin(os.environ["GROOMLAB_PLUGIN"])
        # On MAYA_SCRIPT_PATH when installed. The AE template before any groom is selected: the
        # Attribute Editor builds a node type's layout once, with whatever template exists then.
        for proc in ("groomLabBrushProperties", "groomLabBrushValues", "AEgroomLabShapeTemplate"):
            maya.mel.eval('source "%s/%s.mel"' % (scripts.replace("\\", "/"), proc))
        import groomlab  # noqa: F401
    except Exception:
        with open(os.environ.get("GROOMLAB_GUI_LOG", os.devnull), "w") as log:
            log.write(traceback.format_exc())
        cmds.quit(force=True, exitCode=1)
        return
    # The suites start once Maya's idle work (the plugin's deferred menu install) is done.
    cmds.evalDeferred(_run, lowestPriority=True)


def _run():
    import gui_support
    gui_support.run()


maya.utils.executeDeferred(_load)

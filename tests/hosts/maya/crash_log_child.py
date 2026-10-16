# SPDX-License-Identifier: Apache-2.0
"""A mayapy that crashes on purpose, for test_plugin's crash log cases (Windows only).

Usage: mayapy crash_log_child.py <GroomLab.mll> <marker dir> <mode>
mode "fault": a filter (standing in for Maya's crash handler) is set, then the plugin loads; a
native thread then faults inside ucrtbase.dll (strlen of a null pointer). The plugin's log must name
the fault and the filter before it must still run (it writes <marker dir>/maya_handler).
mode "unloaded": the same filter, the plugin loaded, a second filter set after it (as Arnold sets
one when MtoA starts it, keeping a pointer to the plugin's), then the plugin unloaded. The fault
must still reach the first filter through the plugin's (kept loaded, only handing on).
"""

import ctypes
import os
import sys
import time

PLUGIN_PATH = os.path.abspath(sys.argv[1])
MARKERS = os.path.abspath(sys.argv[2])
MODE = sys.argv[3]
sys.argv = sys.argv[:1]

import maya.standalone  # noqa: E402

maya.standalone.initialize(name="python")

from maya import cmds  # noqa: E402

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.SetUnhandledExceptionFilter.restype = ctypes.c_void_p
kernel32.SetUnhandledExceptionFilter.argtypes = [ctypes.c_void_p]
kernel32.CreateThread.restype = ctypes.c_void_p
kernel32.CreateThread.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32,
                                  ctypes.c_void_p]
kernel32.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
FILTER = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p)
EXCEPTION_EXECUTE_HANDLER = 1


def mark(name):
    with open(os.path.join(MARKERS, name), "w") as f:
        f.write("ran\n")


def maya_handler(pointers):
    mark("maya_handler")
    return EXCEPTION_EXECUTE_HANDLER  # the process ends here, as Maya's handler ends it


maya_filter = FILTER(maya_handler)
kernel32.SetUnhandledExceptionFilter(ctypes.cast(maya_filter, ctypes.c_void_p))
cmds.loadPlugin(PLUGIN_PATH)
try:
    cmds.loadPlugin("mtoa", quiet=True)  # after GroomLab: its version is still read (optional)
    if cmds.pluginInfo("mtoa", query=True, loaded=True):
        mark("mtoa")
except RuntimeError:
    pass

if MODE == "unloaded":
    chained = {}

    def later_handler(pointers):
        mark("later_handler")
        return FILTER(chained["previous"])(pointers)

    later_filter = FILTER(later_handler)
    chained["previous"] = kernel32.SetUnhandledExceptionFilter(ctypes.cast(later_filter, ctypes.c_void_p))
    cmds.flushUndo()
    cmds.unloadPlugin(os.path.basename(PLUGIN_PATH))
    mark("unloaded")

# strlen(NULL) as a thread's start: a read of address 0 inside ucrtbase.dll on a native thread.
strlen = ctypes.cast(ctypes.CDLL("ucrtbase").strlen, ctypes.c_void_p).value
mark("faulting")
thread = kernel32.CreateThread(None, 0, strlen, None, 0, None)
kernel32.WaitForSingleObject(thread, 30000)
time.sleep(10)
mark("survived")  # never: the fault ends the process
os._exit(0)

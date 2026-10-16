# SPDX-License-Identifier: Apache-2.0
"""GroomLab installer, run by install.mel (dragged onto Maya's viewport).

Registers the GroomLab module that sits beside this file with Maya: a GroomLab.mod in the
user's modules folder pointing here, so every session of a Maya it has a build for (2022 to 2027:
GroomLab/maya_versions.json, a MAYAVERSION section of the .mod each) finds it. Then loads this
Maya's plugin into this session (auto-loaded from now on). Arnold picks up the translator and the
procedural after Maya restarts. Uninstall: delete <user maya folder>/modules/GroomLab.mod.
"""

import json
import os
import sys

from maya import cmds

HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")
MODULE = HERE + "/GroomLab"
BUNDLED_MOD = HERE + "/GroomLab.mod"


def _module_lines():
    """The bundled .mod with its relative module path made absolute (this folder)."""
    with open(BUNDLED_MOD) as f:
        lines = [line.rstrip("\n") for line in f]
    out = []
    for line in lines:
        parts = line.split()
        if line.startswith("+") and len(parts) >= 5:
            # "+ MAYAVERSION:2022 GroomLab <version> <path>": the path becomes this folder's.
            out.append(" ".join(parts[:4] + [MODULE]))
        else:
            out.append(line)
    return out


def _version(lines):
    return next((l.split()[3] for l in lines if l.startswith("+") and len(l.split()) >= 5), "?")


def _builds():
    """{"2022": {"status": "tested" | "beta", "plugin", "python", "arnold"}, ...}: the Maya versions
    this package has a build for (Maya 2022's alone in a package without maya_versions.json)."""
    try:
        with open(MODULE + "/maya_versions.json") as f:
            return json.load(f)
    except (IOError, OSError, ValueError):
        return {"2022": {"status": "tested", "plugin": "plug-ins/GroomLab.mll", "python": "python", "arnold": True}}


def install():
    if not os.path.isdir(MODULE):
        cmds.error("GroomLab: no GroomLab folder beside the installer (%s); unzip the whole package first" % MODULE)
    year = str(int(cmds.about(apiVersion=True)) // 10000)
    builds = _builds()
    build = builds.get(year)
    if not build:
        cmds.error("GroomLab: this package has no build for Maya %s (it has Maya %s)"
                   % (cmds.about(version=True), ", ".join(sorted(builds))))
    lines = _module_lines()
    modules = os.path.join(cmds.internalVar(userAppDir=True), "modules").replace("\\", "/")
    if not os.path.isdir(modules):
        os.makedirs(modules)
    target = modules + "/GroomLab.mod"
    if os.path.exists(target):
        with open(target) as f:
            old = f.read()
        if MODULE not in old and cmds.about(batch=True) is False:
            answer = cmds.confirmDialog(title="GroomLab", button=["Replace", "Cancel"], defaultButton="Replace",
                                        cancelButton="Cancel", dismissString="Cancel",
                                        message="Another GroomLab install is registered:\n%s\n\nReplace it with this "
                                                "one?\n%s" % (target, MODULE))
            if answer != "Replace":
                return None
    with open(target, "w") as f:
        f.write("\n".join(lines) + "\n")

    # This session: the Python package, the icons, then the plugin (its menu installs itself).
    for path in (MODULE + "/scripts", MODULE + "/" + build["python"]):
        if path not in sys.path:
            sys.path.insert(0, path)
    os.environ["XBMLANGPATH"] = MODULE + "/icons" + os.pathsep + os.environ.get("XBMLANGPATH", "")
    plugin = MODULE + "/" + build["plugin"]
    if cmds.pluginInfo("GroomLab", query=True, loaded=True):
        loaded = cmds.pluginInfo("GroomLab", query=True, path=True).replace("\\", "/")
        if loaded != plugin:
            cmds.warning("GroomLab: another GroomLab plugin is loaded (%s); restart Maya to use this one" % loaded)
    else:
        cmds.loadPlugin(plugin)
    cmds.pluginInfo(plugin, edit=True, autoload=True)
    arnold = (" Restart Maya once for Arnold rendering of grooms." if build.get("arnold") else
              " This Maya's build has no Arnold translator yet: Arnold renders groom files through the procedural.")
    message = ("GroomLab %s installed from %s.\nThe GroomLab menu is in the main menu bar.%s"
               % (_version(lines), MODULE, arnold))
    if build.get("status") == "beta":
        message += ("\n\nGroomLab for Maya %s is a beta: built for it, not yet tested in it. Please write to "
                    "support@getgroomlab.com with anything that does not work." % year)
    print(message)
    if not cmds.about(batch=True):
        cmds.confirmDialog(title="GroomLab", message=message, button=["OK"])
    return target


if __name__ == "__main__":
    install()

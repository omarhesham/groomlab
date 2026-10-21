GroomLab for Maya 2022 to 2027 (Windows)
========================================

Needs: Maya 2022, 2023, 2024, 2025, 2026 or 2027 on Windows (64-bit). One package holds a build
for each; Maya picks its own. For Arnold renders of grooms: the MtoA that ships with that Maya.
If Maya fails to load the plugin with a missing-DLL error, install the latest "Microsoft Visual
C++ Redistributable for Visual Studio 2015-2022 (x64)".

Maya 2022 is the tested version. The builds for Maya 2023, 2024, 2025, 2026 and 2027 are a beta:
built for each version and checked without it, not yet tested inside it. Please write to
support@getgroomlab.com with anything that does not work there. Maya 2024's build has no Arnold
translator yet: in Maya 2024, Arnold renders grooms saved as groom files (the procedural).

Install
  1. Unzip this package somewhere it can stay (e.g. Documents\GroomLab). Keep the
     GroomLab folder, GroomLab.mod, install.mel and install_groomlab.py together.
  2. Start Maya and drag install.mel onto the viewport.
     - Maya may ask whether to trust the plugin's folder ("Untrusted Plugin Loading"):
       choose Allow (tick "Apply to all plugins in this location" to not be asked again).
     - A GroomLab menu appears in the main menu bar; the plugin loads automatically from
       now on.
     - The one install serves every Maya version above: start another Maya and GroomLab is
       there too (load it once from Windows > Settings/Preferences > Plug-in Manager, or drag
       install.mel onto that Maya as well).
  3. Restart Maya once so Arnold picks up GroomLab's translator and procedural.

Start
  GroomLab > Create Groom from Selection with a head mesh selected, then GroomLab >
  Groom Tools and GroomLab > Graph Editor. The user guide and full reference are in GroomLab\docs
  (USER_GUIDE.md, reference\README.md), and online at https://www.getgroomlab.com/docs.

Update
  GroomLab > Check for Updates...: Update Now downloads the new version, checks it and installs
  it beside this folder; restart Maya to finish (this folder goes once the new version has
  started). Once a day GroomLab also looks for a new version when Maya starts and says so in
  the Groom Tools (turn that off in the same window, or set GROOMLAB_NO_UPDATES=1).
  By hand: unzip the new package and drag its install.mel onto Maya.

Support
  GroomLab is free for any use, commercial included. It is funded by donations: GroomLab >
  Support GroomLab, or https://ko-fi.com/getgroomlab.

Uninstall
  Delete <Documents>\maya\modules\GroomLab.mod (and this folder), or run in Maya's Python
  tab:  import os; from maya import cmds;
        os.remove(os.path.join(cmds.internalVar(userAppDir=True), "modules", "GroomLab.mod"))

Crashes
  GroomLab writes %TEMP%\GroomLabCrash_<date>_<time>.log when Maya goes down: please send
  it along with what you were doing.

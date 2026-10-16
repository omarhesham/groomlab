GroomLab app (beta) for Windows
===============================

The GroomLab app grooms hair, fur and feathers without a 3D package: open a scalp mesh or a
.groom file, groom guides with brushes, style with the node graph, look-dev it in the viewport
and export to Alembic, USD, Unreal, Unity and glTF cards. It runs the same core as the GroomLab
plugin for Maya, so a .groom file looks the same in both.

This is a BETA. It works, but expect rough edges: features may change before the first release,
and keep backups of the grooms you care about. VERSION.txt says which build this is.


Requirements
------------

- Windows 10 or 11, 64-bit (x64).
- A graphics card and driver with Direct3D 11 (Direct3D 12 also works). Any GPU from the last
  ten years does; the viewport does not run on Microsoft's basic display driver over remote
  desktop.
- No installer and no administrator rights. The Microsoft Visual C++ runtime GroomLab needs is
  in this folder. If Windows still reports a missing DLL (MSVCP140.dll, VCRUNTIME140.dll),
  install Microsoft's "Visual C++ Redistributable 2015-2022 (x64)":
  https://aka.ms/vs/17/release/vc_redist.x64.exe


Start
-----

1. Unzip the whole folder anywhere you can write to (Documents, a tools folder; not inside the
   zip viewer).
2. Run GroomLab.exe. Windows SmartScreen may warn about a new download: choose "More info",
   then "Run anyway".
3. File > Open Groom... opens a .groom file; File > New Groom on Mesh... starts one on an .obj or
   .abc mesh. Help > Docs opens the documentation: https://getgroomlab.com/docs/app

To update, download the new zip and unzip it in place of this folder (close GroomLab first).
Help > Check for Updates says whether a newer version is out. Your settings and recent files
live outside this folder and are kept.


Free for any use
----------------

GroomLab is free for any use, commercial included: every feature and every export, no
watermark, no key and no activation. It is funded by donations: if it helps your work, support
it at https://ko-fi.com/getgroomlab (Help > Support GroomLab).


Files and logs
--------------

- Log: %LOCALAPPDATA%\GroomLab\logs\GroomLab.log (the run before: GroomLab.previous.log).
  Paste %LOCALAPPDATA%\GroomLab\logs into Explorer's address bar to open the folder.
- Settings and recent files: %APPDATA%\GroomLab\GroomLab.ini
- Your own graph presets: Documents\maya\groomlab\presets (shared with the Maya plugin).

To uninstall, delete this folder (and the folders above, to remove every trace).

GroomLab connects to the internet only when you choose Help > Check for Updates. Nothing about
your grooms is sent.


Report an issue
---------------

Write to support@getgroomlab.com with:
- what you did and what happened (a screenshot helps),
- VERSION.txt from this folder, your Windows version and graphics card,
- the log, %LOCALAPPDATA%\GroomLab\logs\GroomLab.log (and GroomLab.previous.log after a crash),
- the .groom file and its mesh if you can share them.


Legal
-----

GroomLab is open source under the Apache License 2.0, free to use for any purpose, commercial
included. Copyright 2026 Nick Rutlinh. It uses open-source components, each under its own
license: THIRD_PARTY_NOTICES.txt. The Qt libraries (Qt6*.dll and the plugin folders) are used
under the GNU LGPL version 3: licenses\QT_NOTICE.txt says how to get Qt's source code and how
to use your own build of Qt with GroomLab.

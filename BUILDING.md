# Building GroomLab from source

GroomLab builds with CMake 3.25+ and Visual Studio 2022 on Windows, or GCC 9+ on Linux. The
engine (core, Python module, CLI, C API, Blender helper) needs nothing proprietary. The Maya
plugin, the Arnold parts and the RenderMan procedural build against **your own** installs of
those products. Their SDKs are not redistributable, so they are not in this repository and must
never be committed.

[docs/DEVELOPING.md](docs/DEVELOPING.md) is the full handbook: every preset, the test tiers,
faster builds, debugging Maya. macOS (Apple silicon) is being ported: [docs/MACOS.md](docs/MACOS.md).

## 1. The engine only (no proprietary SDK)

```bash
cmake --preset win-core      # or linux-core
cmake --build --preset win-core
ctest --preset win-core
```

This builds the core library, the `groomlab_core` Python module, `groomlab-cli`, the C API and
`groomlab-helper`. The first configure needs network access: FetchContent pulls pybind11,
GoogleTest, nlohmann/json, Imath, Alembic and (on Windows) the Slang compiler. Each is pinned by
tag or hash ([THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)). On Linux, install TBB
(`libtbb-dev`) for parallel loops. Without it the loops run serially.

## 2. The Maya plugin, with your Maya

| What you have | Point the build at it |
| --- | --- |
| Maya 2022 installed | `MAYA_LOCATION` (default in the presets: `C:/Program Files/Autodesk/Maya2022`) |
| Maya 2023-2027 devkit (free from the [Autodesk Platform Services](https://aps.autodesk.com/developer/overview/maya) site) | unzip it anywhere and set `MAYA_LOCATION` to it |

```bash
cmake --preset win-maya2022                                   # installed Maya 2022
cmake --preset ninja-maya2025 -DMAYA_LOCATION=C:/sdks/maya-devkit-2025   # a devkit (example path)
```

The `ninja-maya2023` to `ninja-maya2027` presets expect each devkit under
`%USERPROFILE%/groomlab-deps/sdks/maya-devkit-<year>`, and the matching Python under
`%USERPROFILE%/groomlab-deps/sdks/python-<version>/tools` (`GROOMLAB_PYTHON_ROOT`). Override any
of these on the command line or in a `CMakeUserPresets.json`, which git ignores. TBB comes from
the same Maya or devkit.

## 3. Arnold, with your MtoA

Set `MTOA_LOCATION` to an MtoA install (it ships the Arnold and MtoA SDK headers and libraries),
for example `C:/Program Files/Autodesk/Arnold/maya2022`. With it, the build adds the MtoA
translator (`GROOMLAB_BUILD_ARNOLD`) and the `groomlab_procedural` Arnold plugin
(`GROOMLAB_BUILD_PROCEDURAL`). Without it, both are skipped. Rendering tests use `kick` from the
same install. Without an Arnold license, renders carry Arnold's watermark, which is fine for the
tests.

## 4. RenderMan, with your RenderMan Pro Server

Set `GROOMLAB_RMANTREE` (or the `RMANTREE` environment variable) to a RenderMan Pro Server
install. The non-commercial edition works. The build adds the `groomlab_renderman` procedural
(`GROOMLAB_BUILD_RENDERMAN`). Without it, the procedural is skipped.

## 5. The standalone app, with your Qt

`-DGROOMLAB_BUILD_APP=ON -DGROOMLAB_QT_DIR=<Qt 6.8 kit>` (for example
`.../Qt/6.8.3/msvc2022_64`, from the Qt online installer under the open-source LGPL terms), or
use the `ninja-app` preset. The app links Qt dynamically. If you distribute it, follow the LGPL
obligations in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## 6. The Blender add-on

The add-on (`hosts/blender/addon`, GPL-3.0-or-later) is pure Python. It starts
`groomlab-helper`, which section 1 builds. `python tools/package_blender.py` assembles the
extension zip. See [docs/BLENDER.md](docs/BLENDER.md).

## Keeping SDKs out of the repository

- Keep SDKs and devkits outside the source tree (the presets use `%USERPROFILE%/groomlab-deps`).
  Put machine-specific paths in `CMakeUserPresets.json` or environment variables, never in
  committed files.
- Never commit anything from a Maya devkit, MtoA/Arnold, RenderMan, Unreal or Unity
  (headers, `.lib`/`.dll`/`.so`, sample scenes), and no assets you don't have the right to
  redistribute (scans, purchased models, MetaHuman/Megascans content).

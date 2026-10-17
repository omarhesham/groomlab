# Contributing to GroomLab

Thanks for helping. Bug reports, presets, docs fixes and code are all welcome.

## Before you start

- **Bugs:** open an issue with the GroomLab version, the host and its version (Maya, Blender, the
  app), the OS, what you did, and what happened. A small `.groom` file or scene helps most.
- **Bigger changes** (a new node, a file-format change, a new host): open an issue first so we can
  agree on the shape before you write it.

## Making a change

1. Build and test as [BUILDING.md](BUILDING.md) describes. The engine-only presets
   (`win-core`, `linux-core`) need no proprietary SDK.
2. Follow the conventions in [docs/DEVELOPING.md](docs/DEVELOPING.md), section 6 ("Rules").
   Format C++ with the repository's `.clang-format`. Stored formats change in place, with no
   migrations. Add or update tests with the change. Golden files change only for intended
   output changes.
3. Adding a node: [docs/EXTENDING.md](docs/EXTENDING.md) (`tools/dev/new_node.py` makes it, then
   params, tests, the GPU preview and the docs).
4. Keep one topic per pull request. The commit message says what changed and why.

## Math

Where the math lives (one library for the nodes, the expressions and the GPU kernels) and how to
add a function or an expression builtin: [docs/MATH.md](docs/MATH.md).

## Licensing of contributions

GroomLab is licensed under the [Apache License 2.0](LICENSE). Under its section 5, anything you
submit for inclusion is licensed under the same terms. Contributions to `hosts/blender/addon/`
are licensed under the GPL-3.0-or-later, like that folder.

New source files start with an SPDX line, for example
`// SPDX-License-Identifier: Apache-2.0` (C++), or `# SPDX-License-Identifier: GPL-3.0-or-later`
in the Blender add-on.

Never commit files you don't have the right to redistribute. That includes Maya devkit, Arnold,
RenderMan, Unreal or Unity SDK files, scans or models under restrictive licenses, and anything
from your employer or clients.

## Security issues

Don't open a public issue. See [SECURITY.md](SECURITY.md).

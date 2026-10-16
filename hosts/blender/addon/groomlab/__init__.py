# SPDX-License-Identifier: GPL-3.0-or-later
#
# GroomLab for Blender: groom with Blender's own hair curves and Curves Sculpt brushes while
# GroomLab's node graph grows, shapes and colors the hair from them.
#
# This add-on is free software: you can redistribute it and/or modify it under the terms of the
# GNU General Public License as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version (see LICENSE).
#
# It drives groomlab-helper (bin/), a separate program (Apache-2.0) it starts and talks to over
# pipes and shared memory (docs/BLENDER.md). Nothing of the helper is loaded into Blender.

from . import (display, graph_editor, graph_tree, handlers, icons, menu, ops, palette, process, props, tool_ops,
               tool_props, tools, ui)


def register():
    props.register()
    tool_props.register()
    icons.register()
    ops.register()
    ui.register()
    tool_ops.register()
    palette.register()
    menu.register()
    tools.register()
    display.register()
    graph_tree.register()
    graph_editor.register()
    handlers.register()


def unregister():
    from . import groom
    handlers.unregister()
    display.unregister()
    tools.unregister()
    menu.unregister()
    palette.unregister()
    tool_ops.unregister()
    graph_editor.unregister()
    graph_tree.unregister()
    ui.unregister()
    ops.unregister()
    icons.unregister()
    tool_props.unregister()
    props.unregister()
    groom._runtimes.clear()
    process.shutdown()

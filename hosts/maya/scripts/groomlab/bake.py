# SPDX-License-Identifier: Apache-2.0
"""Bake a graph node's output into the guide hierarchy.

Procedural guides (a ToGuides node picking strands, a Mirror of guides) live in
the graph and change with it. Baking them adds the curves the
node makes right now as primary guides of a root group, so they can be brushed,
given children, mirrored and saved like guides drawn by hand.

    from groomlab import bake
    bake.bake_guides(groom, "toGuides", group="baked")

The node's output must be Guides or Strands (each strand becomes a guide). One
undo step (groomLabGuides -fromNode).
"""


def bake_guides(groom, node, group="primary", density=1.0, tube_radius=0.1, tube_tip_radius=None):
    """Add the guides `node` outputs (evaluated at `density`) to root group `group`.

    Returns the new guide IDs (decimal strings). Raises RuntimeError when the node
    is missing or does not output guides or strands.
    """
    from maya import cmds

    kwargs = {"fromNode": node, "group": group, "density": density, "tubeRadius": tube_radius}
    if tube_tip_radius is not None:
        kwargs["tubeTipRadius"] = tube_tip_radius
    return list(cmds.groomLabGuides(groom, **kwargs) or [])

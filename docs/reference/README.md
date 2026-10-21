# GroomLab reference

The deep technical reference: every node, parameter and tool as GroomLab in Maya has it, with the
details the website leaves out (how each operator computes, defaults, edge cases, Python calls).
Most of it applies to the app and Blender too, which run the same engine. For a walkthrough start
with the website's docs, https://www.getgroomlab.com/docs, or the [user guide](../USER_GUIDE.md);
each file below names its matching website page at the top.

| File | What it covers |
| --- | --- |
| [getting-started.md](getting-started.md) | Installing a build into Maya, a first groom from Python, how the preview, Scatter, Interpolate and Resample behave, the graph attribute |
| [guides-and-brushes.md](guides-and-brushes.md) | The guide hierarchy, parting, guide selection, the brushes, grooming from reference photos, flow arrows, correctives |
| [looks-and-maps.md](looks-and-maps.md) | The procedural look, color patterns, painted and texture maps, the viewport look, themes |
| [nodes.md](nodes.md) | Every operator by category, and the look, shape and deform operators one row each |
| [nodes-and-fields.md](nodes-and-fields.md) | Expressions, fields, instancing geometry on strands, procedural guides, sculpting generated guides |
| [graph-editor.md](graph-editor.md) | The Graph Editor, Groom Tools, the shipped presets (the full table) and the GroomLab menu |
| [feathers-braids-scales.md](feathers-braids-scales.md) | Braids, scales, feathers |
| [rendering.md](rendering.md) | Review, Arnold (and groom files on a farm), RenderMan |
| [game-export.md](game-export.md) | Game cards, feather cards, Resample by length or curvature, Unreal, Unity, glTF |
| [pipeline.md](pipeline.md) | Alembic and USD export, where a scene's files live, paths and referencing, rest mesh, follow the scalp, colliders, several scalps, linked grooms, transfer, utilities, groom states |
| [motion-and-simulation.md](motion-and-simulation.md) | Wind and Jiggle, simulating the guides, guides from Alembic, the simulation round trip |
| [cli.md](cli.md) | Groom files, `groomlab-cli`, importing and reloading groom files |
| [conventions.md](conventions.md) | The rules the code keeps |

Elsewhere in `docs/`: [DEVELOPING.md](../DEVELOPING.md) (the developer handbook),
[EXTENDING.md](../EXTENDING.md) (adding a node), [SPEC.md](../SPEC.md) (the spec and roadmap),
[SESSION.md](../SESSION.md), [UI_DEFINITIONS.md](../UI_DEFINITIONS.md),
[UNREAL_HAIR_MATERIAL.md](../UNREAL_HAIR_MATERIAL.md).

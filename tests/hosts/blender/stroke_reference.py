# SPDX-License-Identifier: Apache-2.0
"""The strokes blender.helper sends the helper, replayed by the core itself (the groomlab_core
bindings: BrushTool over a StrokeHost of a GroomSession, what the standalone app's BrushSession
drives), so the helper's results can be checked against them bit for bit.

    <python with groomlab_core> stroke_reference.py spec.json out.json

spec.json: {"groom": a .groom the helper saved before the steps, "matrix": the view's world-to-clip
matrix in groom space (the core's convention, as the helper answered it), "width", "height",
"scalpMaps": the session's scalp maps text (or null), "steps": [{"op": "stroke", "settings":
{StrokeSettings keywords}, "events": [[x, y, pressure], ...], "ctrl", "shift"} | {"op":
"map_for_param", "node", "param"} (a setting's map made and linked: paint_param_map) | {"op": "select",
"ids", "soft_radius"} | {"op": "hidden", "groups"} | {"op": "undo"} | {"op": "redo"}]}.
out.json: the stored texts after each step ({guideData, partData, flowData, scalpMaps}).

Runs under the Python the bindings were built for (Maya's mayapy for the Maya presets: plain
Python here, no Maya), so it keeps to Python 3.7.
"""

import json
import sys

import groomlab_core as gl


def main(spec_path, out_path):
    with open(spec_path) as f:
        spec = json.load(f)
    session = gl.GroomSession.from_groom_file(spec["groom"])
    if spec.get("scalpMaps"):
        session.set_scalp_maps_json(spec["scalpMaps"])
    host = gl.StrokeHost(session)
    view = gl.ToolView(spec["matrix"], spec["width"], spec["height"])
    tool = gl.BrushTool()
    out = []
    for step in spec["steps"]:
        op = step["op"]
        if op == "stroke":
            tool.settings = gl.StrokeSettings(**step["settings"])
            events = step["events"]
            x, y, p = events[0]
            tool.press(host, view, x, y, p, step.get("ctrl", False), step.get("shift", False))
            for x, y, p in events[1:]:
                tool.drag(host, view, x, y, p)
            tool.release(host, view)
        elif op == "map_for_param":
            gl.paint_param_map(host, tool.settings, step["node"], step["param"])
        elif op == "select":
            host.set_selection(step["ids"], step.get("soft_radius", 0.0))
        elif op == "hidden":
            host.set_hidden_groups(set(step["groups"]))
        elif op == "undo":
            host.undo()
        elif op == "redo":
            host.redo()
        else:
            raise ValueError("unknown step " + op)
        data = session.data
        out.append({"guideData": data.hierarchy_json, "partData": data.parts_json, "flowData": data.flow_json,
                    "scalpMaps": session.scalp_maps_json, "warnings": list(host.warnings)})
        host.clear_warnings()
    with open(out_path, "w") as f:
        json.dump(out, f)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])

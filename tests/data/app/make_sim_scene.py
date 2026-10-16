# SPDX-License-Identifier: Apache-2.0
"""Writes the simulation check's scene: head_scalp.obj turning its head as a scalp cache.

The head (head_scalp.obj beside this script) is still for frames 1-4, turns 35 degrees to its
left by frame 14, 25 to its right by frame 22, back to the front by frame 30 and holds still to
frame 72: the hair of sim_head_graph.json (a Simulate node on guides grown from the scalp)
swings with the turns and settles after. Run with mayapy and the build's groomlab_core on the
PYTHONPATH, then open the cache in the app with the graph:

    set PYTHONPATH=build/maya/engine/python/RelWithDebInfo
    mayapy tests/data/app/make_sim_scene.py %TEMP%/sim_head.abc
    GroomLab %TEMP%/sim_head.abc --graph tests/data/app/sim_head_graph.json --time 18 --screenshot f18.png
"""

import math
import os
import sys

import groomlab_core as gl

HERE = os.path.dirname(os.path.abspath(__file__))
FRAMES = 72
PIVOT = (0.0, -12.0, 0.0)  # the neck


def read_obj(path):
    points, triangles = [], []
    with open(path) as f:
        for line in f:
            words = line.split()
            if not words:
                continue
            if words[0] == "v":
                points.append(tuple(float(w) for w in words[1:4]))
            elif words[0] == "f":
                corners = [int(w.split("/")[0]) - 1 for w in words[1:]]
                for k in range(1, len(corners) - 1):
                    triangles.append((corners[0], corners[k], corners[k + 1]))
    return points, triangles


def smooth(a, b, t):
    t = min(max((t - a) / (b - a), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def yaw_at(frame):
    """The head's turn about y (degrees) at a frame: left, right, back to the front, still."""
    return (35.0 * smooth(4, 14, frame) - 60.0 * smooth(14, 22, frame) + 25.0 * smooth(22, 30, frame))


def turned(points, degrees):
    a = math.radians(degrees)
    c, s = math.cos(a), math.sin(a)
    out = []
    for x, y, z in points:
        dx, dz = x - PIVOT[0], z - PIVOT[2]
        out.append((PIVOT[0] + c * dx + s * dz, y, PIVOT[2] - s * dx + c * dz))
    return out


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "sim_head.abc")
    points, triangles = read_obj(os.path.join(HERE, "head_scalp.obj"))
    with gl.AlembicMeshWriter(out, "head", 1.0 / 24.0, 1.0 / 24.0) as writer:
        for frame in range(1, FRAMES + 1):
            writer.write_frame(gl.ScalpMesh(turned(points, yaw_at(frame)), triangles))
    print("wrote %s: %d frames" % (out, FRAMES))


if __name__ == "__main__":
    main()

# SPDX-License-Identifier: GPL-3.0-or-later
"""Starter guides for a new groom: evenly spread over the scalp (farthest-point picks among
area-weighted samples, the same every time), each leaving the skin along its normal and bending
toward the ground, ready to comb with Blender's Curves Sculpt brushes. numpy only."""

import numpy as np


def fan_triangles(counts, indices):
    """(T, 3) corner indices of the polygons' fans from their first corner (as the helper makes them)."""
    counts = np.asarray(counts, np.int64)
    indices = np.asarray(indices, np.int64)
    starts = np.concatenate([[0], np.cumsum(counts)[:-1]])
    tri_per = counts - 2
    poly = np.repeat(np.arange(len(counts)), tri_per)
    k = np.arange(int(tri_per.sum())) - np.repeat(np.cumsum(tri_per) - tri_per, tri_per)
    first = starts[poly]
    return np.stack([indices[first], indices[first + k + 1], indices[first + k + 2]], axis=1)


def _unit(v):
    n = np.linalg.norm(v, axis=-1, keepdims=True)
    return v / np.where(n > 1e-12, n, 1.0)


def starter_guides(points, counts, indices, vertex_normals=None, count=200, length=0.1, segments=8,
                   down=(0.0, 0.0, -1.0), bend_degrees=90.0, seed=7):
    """Returns (points (count*segments, 3) float32, counts (count,) int32) in the scalp's space."""
    points = np.asarray(points, np.float64)
    tris = fan_triangles(counts, indices)
    if len(tris) == 0:
        return np.zeros((0, 3), np.float32), np.zeros(0, np.int32)
    a, b, c = points[tris[:, 0]], points[tris[:, 1]], points[tris[:, 2]]
    cross = np.cross(b - a, c - a)
    area = 0.5 * np.linalg.norm(cross, axis=1)
    if area.sum() <= 0.0:
        return np.zeros((0, 3), np.float32), np.zeros(0, np.int32)
    rng = np.random.default_rng(seed)
    candidates = max(count * 12, 64)
    t = rng.choice(len(tris), size=candidates, p=area / area.sum())
    r1, r2 = rng.random(candidates), rng.random(candidates)
    s = np.sqrt(r1)
    u, v, w = 1.0 - s, s * (1.0 - r2), s * r2
    roots = u[:, None] * a[t] + v[:, None] * b[t] + w[:, None] * c[t]
    face_n = _unit(cross[t])
    if vertex_normals is not None and len(vertex_normals) == len(points):
        vn = np.asarray(vertex_normals, np.float64)
        normals = u[:, None] * vn[tris[t, 0]] + v[:, None] * vn[tris[t, 1]] + w[:, None] * vn[tris[t, 2]]
        normals = np.where(np.linalg.norm(normals, axis=1, keepdims=True) > 1e-9, _unit(normals), face_n)
    else:
        normals = face_n

    # Farthest-point picks: an even spread, whatever the mesh's triangle sizes.
    count = min(count, candidates)
    chosen = [0]
    dist = np.linalg.norm(roots - roots[0], axis=1)
    for _ in range(1, count):
        i = int(np.argmax(dist))
        chosen.append(i)
        dist = np.minimum(dist, np.linalg.norm(roots - roots[i], axis=1))
    roots, normals = roots[chosen], normals[chosen]

    # Each guide leaves along its normal and bends toward the ground (backward on the crown).
    g = _unit(np.asarray(down, np.float64))
    tangent = g[None, :] - (normals @ g)[:, None] * normals
    weak = np.linalg.norm(tangent, axis=1) < 0.2
    if weak.any():
        back = np.cross([1.0, 0.0, 0.0], g) if abs(g[0]) < 0.9 else np.cross([0.0, 1.0, 0.0], g)
        alt = back[None, :] - (normals @ back)[:, None] * normals
        tangent[weak] = alt[weak]
    tangent = _unit(tangent)
    segments = max(int(segments), 2)
    step = length / (segments - 1)
    out = np.empty((len(roots), segments, 3))
    out[:, 0] = roots
    for k in range(1, segments):
        theta = np.radians(bend_degrees) * ((k - 0.5) / (segments - 1)) ** 0.9
        d = np.cos(theta) * normals + np.sin(theta) * tangent
        out[:, k] = out[:, k - 1] + step * d
    return out.reshape(-1, 3).astype(np.float32), np.full(len(roots), segments, np.int32)

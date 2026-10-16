# Interpolate along the skin: frames carried over the scalp, followSurface

What changed: Interpolate carries each guide's shape to its strands along the scalp
(`engine/core/src/surface_paths.cpp`, `engine/core/src/operators/interpolate.cpp`), and with
`followSurface` (on by default) weighs guides by the distance along the scalp.

## The frame transfer

A guide's shape goes from its root to the strand's by parallel transport along the path between
them on the skin. Where the straight line between the roots hugs the skin (shallow at both ends,
normals not facing apart, no detour round a gap), that is the shortest turn between the two
normals: on a sphere exactly the transport along the great circle, on a cylinder the turn about
its axis, elsewhere the same to third order over a guide's reach. So it is the turn Interpolate
made before, to the bit. Where the path bends (round the rim of a thin flap, the fold of a shell)
and the two ends' normals face apart by more than about 145 degrees (where the plain turn is a
guess: opposite normals turn half way round an arbitrary axis), the frame (normal and a tangent)
is carried through each bend and the record keeps the twist it adds.

Not chosen:

- **The UV tangent (dPdu):** seams where islands are cut or flipped, poles where a lat-long
  layout converges (the app's head scalp has one at the crown), noise on a scan's UVs.
- **Any smooth tangent field over the scalp** (diffused, transported from one source, a world
  axis projected): by Gauss-Bonnet such a field turns against parallel transport by the
  curvature it encloses. Round a head's hairline that is about 0.7 / R radians per cm (some
  4-5 degrees a centimetre on a 9 cm head), so every guide would twist by that on its way to its
  strands, or the field has a singular point (a whorl in the wrong place) where it twists by up
  to 180 degrees. Either is worse than the old turn, which has none of it.
- **The guides' own root directions blended:** it moves with every dab (every strand's frame
  near a reshaped guide changes), and on a plane it is not the identity.

## followSurface

A front from each guide root (Dijkstra over the welded rest scalp, distances as straight
segments between bends) out to the guide's reach rounded up to an eighth of an octave; a strand
reads its candidates' paths at its root's triangle. Per triangle the binding knows how many
fronts do not reach it straight within their reach: zero almost everywhere, and there the path is
the straight line without a lookup. The Euclidean search stays the candidate filter. Fronts are
kept with the binding (a dab that moves roots makes only the moved guides' and those whose reach
changed) and on the scalp's graph by root and reach (another binding of the same guides: the
stroke document's, a density change, makes none).

On a smooth head the result is the straight line's: points of the 100k and 500k strands on the
app's head and the bench head are at most 7.6e-6 cm apart with followSurface on and off.

## Cost

Machine: the 24-thread workstation, quiet (2-5% load) for the classic budgets, 6-30% for the
scene runs (other agents building; those were interleaved with the old code path, min of 3).

`groomlab_tests --gtest_filter=Budget.InterpolateHalfMillion` (500k strands, 500 guides, a
sphere of radius 10 with a rest pose), before -> after:

| | before | after |
| --- | --- | --- |
| bound again (new node: binding and paths) | 32.6-32.9 ms (352 one thread) | 34.5-37.7 ms (363-370) |
| deforming frame (binding kept) | 20.2-20.6 ms (216) | 21.6-22.2 ms (219-226) |
| brush dab (binding kept) | 20.6-20.8 ms (217) | 21.2-23.1 ms (218-226) |

`Budget.BrushDabInterpolate5kGuides` (5000 guides, 50k strands): a dab moving roots (children
follow their parents) 4.9 -> 5.3 ms; posed guides (roots kept) 10.0-10.4 -> 10.3-10.9 ms; the very
first evaluation 10-11 -> 26-27 ms (the scalp's graph and 4500 fronts made once).

`Budget.InterpolateFollowingTheSurface` (new): Interpolate on the app's head scalp
(`tests/data/app/head_scalp.obj`, 1225 vertices) and the bench head (9016 vertices), guides one
per 1.6 cm cell (285 and 508), ms:

| scene | strands | very first (graph + paths made) | first (new node) on / off / old | dab (roots kept) | 3 roots moved |
| --- | --- | --- | --- | --- | --- |
| app head | 100k | 14.9-18.8 (old 13.3-15.5) | 8.3-9.1 / 8.2-9.0 / 8.4-9.2 | 1.7-2.3 (old 1.7-2.2) | 5.3-5.6 (old 5.2-6.3) |
| app head | 500k | | 34.7-37.6 / 34.4-37.1 / 33.9-36.3 | 6.8-9.0 (old 7.3-8.5) | 17.9-20.6 (old 18.3-19.1) |
| bench head | 100k | 17.3-24.0 (old 11.3-12.3) | 9.4-11.5 / 9.2-11.3 / 9.3-9.7 | 4.8-5.2 (old 4.8-5.6) | 4.0-5.0 (old 4.2-4.8) |
| bench head | 500k | | 35.7-39.1 / 36.2-39.8 / 35.8-37.6 | 16.7-17.8 (old 16.4-17.2) | 12.9-14.7 (old 13.4-14.0) |

"old" ran the same build with the paths switched off in code (no fronts, no twists: the
previous behavior). The paths cost on a new node's first evaluation 1-2 ms (500 guides on a
9k-vertex scalp; 0.3-0.7 ms for 285 on 1.2k) side by side with the binding's search cells, and
the scalp's graph once per scalp (7 ms for 16k vertices); a dab that keeps the roots costs
nothing more, one that moves a few roots well under a millisecond. followSurface on or off costs
the same (the frames need the paths either way).

## Before / after

`python tools/bench/xgen_compare/make_flap_scene.py --out flap.groom` (and `--follow off`): a
thin flap (0.3 cm, 6 cm high) standing on a floor, guides on its -z face combed out and down,
on its +z face combed along it, on the floor straight up; colored by the guides' side (blue,
green, red), so a strand blending two sides shows a color between them.

```
GroomLab.exe flap.groom --screenshot flap.png --size 1200x800 --orbit 205 18 --distance 24 --look-at 0,3,0 --display guides=0 --display colorMode=attribute --display colorSource=side --display colorMin=0 --display colorMax=2
```

Before (the straight line, one turn between normals): the -z face's hair takes the +z face's
guides through the flap (teal: half one side, half the other) and sticks out of it, turned half
way round an arbitrary axis. After, followSurface on: each face's hair is its own guides' (blue
on the -z face, combed out and down; green on the +z face, lying along it), and only the hair at
the rim, within reach over it, blends the two, carried over the rim. followSurface off: the
straight-line weights blend the faces again (the frames are the new ones).

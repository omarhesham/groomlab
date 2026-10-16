# Card fidelity: color patterns (2026-10-04)

Two grooms of `fidelity_maya.py` that carry HairColor's color patterns, carded at the three
levels of detail and measured against their strands (DirectX 11, 720 px, the hair cap on):

- `banded`: the short fur with agouti banding: guard hairs (40%, at random) dark at the root, a pale
  band at 55-72% of the length (shifted 12% of a period hair to hair), a black tip; the rest a plain
  undercoat.
- `barred`: contour feathers lying on the head (a down zone at the base), white bars across the left
  vane, an iridescent green right vane (`iridescenceOn` `rightVane`), carded per feather.

```bash
python tools/bench/card_fidelity/run_fidelity.py run --out <folder> --build build/maya --grooms banded,barred
```

Means over the five views (the per-view rows are in the JSON beside this file):

| groom | lod | area | outside | iou | seeThrough | dE | dE_lo | dE_roots | dE_mid | dE_tips | dL | cards | tris |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| banded | hero | 0.94 | 0.01 | 0.89 | 0.09 | 19.7 | 7.7 | 20.1 | 19.6 | 19.6 | 4.8 | 833 | 49980 |
| banded | mid | 0.95 | 0.02 | 0.88 | 0.09 | 20.1 | 8.4 | 20.3 | 20.0 | 20.1 | 5.2 | 468 | 14976 |
| banded | low | 0.94 | 0.01 | 0.87 | 0.10 | 20.3 | 8.6 | 20.8 | 20.3 | 20.2 | 5.3 | 400 | 4000 |
| barred | hero | 1.09 | 0.12 | 0.54 | 0.27 | 36.0 | 18.1 | 35.3 | 36.3 | 36.5 | -2.8 | 195 | 4680 |
| barred | mid | 1.09 | 0.12 | 0.53 | 0.27 | 35.4 | 17.3 | 34.7 | 35.5 | 35.9 | -2.2 | 195 | 2340 |
| barred | low | 0.90 | 0.12 | 0.40 | 0.46 | 37.2 | 22.0 | 36.8 | 37.1 | 37.8 | -6.8 | 195 | 780 |

What they say:

- **The silhouette holds**: the banded fur's cards cover 94-95% of its strands' area with almost
  nothing outside (the plain `fur` groom: 96-97%), at every level.
- **The bands bake** (the Color map shows the pale band and the black tips where they are along the
  strands), but per pixel the cards miss the coat's salt-and-pepper: dE 20 against the plain fur's 5,
  blurred 8 against 3. The black tips are a few strands in ten a card bakes at their own width,
  where the viewport draws every strand at least a pixel wide: the cards read 5 points lighter (dL)
  and the speckle averages out. A pattern of high contrast at the strand's scale is what cards lose.
- **Feathers:** the bars bake onto the left half of each feather's card, the vane laid flat, and
  the green iridescent vane draws on the right half (the viewport reads the bake's iridescence, the
  Color map's alpha, per texel; a per-card mean across the vertices first tried tinted both vanes
  green and hid the bars, at about the same dE, 34: per pixel the dE punishes the bars' contrast
  either way). The feather cards' fit is what limits them: IoU 0.5, a quarter of the strands'
  area seen through, a tenth outside; the low level (one quad a segment) drops to IoU 0.4. The
  iridescence turns with each card's normal while the strands turn with each barb's frame, so
  views edge-on to the cards (side, back) differ most in hue.

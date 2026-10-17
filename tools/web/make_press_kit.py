# SPDX-License-Identifier: Apache-2.0
"""The website's press kit (web/public/press): the GroomLab logo as SVG and PNG, and one zip of it all.

    python tools/web/make_press_kit.py [--font PATH]

Three pieces (the mark, the wordmark, the mark and wordmark together) in three versions (color, black,
white), plus the lockup in color for dark backgrounds. Each as SVG and as PNG 512 and 2048 px wide on a
transparent background, then groomlab-press-kit.zip with every file and a README.txt.

The artwork is the shipped logo, nothing redrawn:
- the mark is hosts/app/icons/ui_logo.svg (the same strands as web/components/logo.tsx), read from that
  file, in its own 30 x 30 box;
- the wordmark is "GroomLab" as the site header sets it (web/components/logo.tsx: Inter bold,
  tracking-tight, 1.05rem beside the 28 px mark with a 0.5rem gap, the strands' roots on the baseline),
  turned into outlines with fontTools, so the SVGs need no font. The font is the Inter the site ships
  (next/font): by default the first Inter woff2 under web/.next/static/media or web/out/_next/static/media
  that has the letters, so run `npm run build` in web/ once first, or pass --font.

PNGs are rasterized by sharp from web/node_modules (`npm ci` in web/), through node. Needs fontTools
(with brotli for woff2). Output is deterministic: the zip has fixed timestamps.
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys
import zipfile

from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WEB = os.path.join(ROOT, "web")
OUT = os.path.join(WEB, "public", "press")
MARK_SVG = os.path.join(ROOT, "hosts", "app", "icons", "ui_logo.svg")

NAME = "GroomLab"
INK = "#1D1D1F"  # the site's text color (gl-ink), the wordmark's color on light backgrounds
BLACK = "#000000"
WHITE = "#FFFFFF"

# The header's type, in the mark's units (the mark's 30-unit box is drawn 28 px high).
PX = 30.0 / 28.0
FONT_SIZE = 16.8 * PX  # 1.05rem
TRACKING = -0.025 * FONT_SIZE  # tracking-tight
GAP = 8.0 * PX  # gap-2
BASELINE = 25.0  # the strands' roots
WEIGHT = 700  # font-bold
OPSZ = 16.8  # font-optical-sizing: auto (the CSS px size)
STROKE = 3.0
PAD = 1.5  # transparent margin around the wordmark and lockup, so nothing touches the edge

WIDTHS = (512, 2048)


def num(v):
    s = f"{v:.3f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def read_mark():
    """The strands (path data and color) from the app's logo file."""
    with open(MARK_SVG, encoding="utf-8") as f:
        svg = f.read()
    strands = re.findall(r'<path d="([^"]+)" stroke="(#[0-9A-Fa-f]{6})"', svg)
    if len(strands) != 3:
        sys.exit(f"expected three strands in {MARK_SVG}, found {len(strands)}")
    return [(d, c.upper()) for d, c in strands]


def mark_bounds(strands):
    """The strands' box with their round caps (each strand rises monotonically, so its ends bound it)."""
    xs, ys = [], []
    for d, _ in strands:
        v = [float(n) for n in re.findall(r"-?\d+(?:\.\d+)?", d)]
        for x, y in ((v[0], v[1]), (v[-2], v[-1])):
            xs += [x - STROKE / 2, x + STROKE / 2]
            ys += [y - STROKE / 2, y + STROKE / 2]
    return min(xs), min(ys), max(xs), max(ys)


def find_font(explicit):
    if explicit:
        return explicit
    pattern = [
        os.path.join(WEB, ".next", "static", "media", "*.woff2"),
        os.path.join(WEB, "out", "_next", "static", "media", "*.woff2"),
    ]
    for path in sorted(p for pat in pattern for p in glob.glob(pat)):
        font = TTFont(path)
        cmap = font.getBestCmap() or {}
        if "fvar" in font and font["name"].getDebugName(1) == "Inter" and all(ord(c) in cmap for c in NAME):
            return path
    sys.exit("no Inter woff2 found: run `npm run build` in web/ first, or pass --font")


def kerning(font, left, right):
    """The GPOS 'kern' adjustment between two glyphs (pair positioning, formats 1 and 2)."""
    gpos = font["GPOS"].table
    lookups = sorted({i for fr in gpos.FeatureList.FeatureRecord if fr.FeatureTag == "kern" for i in fr.Feature.LookupListIndex})
    total = 0
    for li in lookups:
        lookup = gpos.LookupList.Lookup[li]
        for st in lookup.SubTable:
            if lookup.LookupType == 9:
                st = st.ExtSubTable
            if st.LookupType != 2 or left not in st.Coverage.glyphs:
                continue
            if st.Format == 1:
                pairs = st.PairSet[st.Coverage.glyphs.index(left)].PairValueRecord
                rec = next((p for p in pairs if p.SecondGlyph == right), None)
                if rec is None:
                    continue
                total += getattr(rec.Value1, "XAdvance", 0) or 0
            else:
                c1 = st.ClassDef1.classDefs.get(left, 0)
                c2 = st.ClassDef2.classDefs.get(right, 0)
                total += getattr(st.Class1Record[c1].Class2Record[c2].Value1, "XAdvance", 0) or 0
            break  # the first subtable that covers the first glyph decides
    return total


def wordmark(font_path, x0):
    """'GroomLab' as one outline path, its pen starting at x0 on the baseline; returns (d, bounds)."""
    font = instancer.instantiateVariableFont(TTFont(font_path), {"wght": WEIGHT, "opsz": OPSZ})
    upm = font["head"].unitsPerEm
    cmap = font.getBestCmap()
    glyphs = font.getGlyphSet()
    hmtx = font["hmtx"]
    s = FONT_SIZE / upm
    names = [cmap[ord(c)] for c in NAME]
    pen = SVGPathPen(glyphs, ntos=num)
    bounds = BoundsPen(glyphs)
    x = x0
    for i, g in enumerate(names):
        t = (s, 0, 0, -s, x, BASELINE)
        glyphs[g].draw(TransformPen(pen, t))
        glyphs[g].draw(TransformPen(bounds, t))
        x += hmtx[g][0] * s + TRACKING
        if i + 1 < len(names):
            x += kerning(font, g, names[i + 1]) * s
    return pen.getCommands(), bounds.bounds


def svg_doc(box, body, label):
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{num(w * 10)}" height="{num(h * 10)}" '
        f'viewBox="{num(x0)} {num(y0)} {num(w)} {num(h)}" role="img" aria-label="{label}">\n'
        f"  <title>{label}</title>\n{body}</svg>\n"
    )


def strands_svg(strands, color):
    paths = "".join(f'    <path d="{d}" stroke="{color or c}"/>\n' for d, c in strands)
    return f'  <g fill="none" stroke-width="{num(STROKE)}" stroke-linecap="round">\n{paths}  </g>\n'


def text_svg(d, color):
    return f'  <path fill="{color}" d="{d}"/>\n'


def pad(box, p=PAD):
    return box[0] - p, box[1] - p, box[2] + p, box[3] + p


def union(a, b):
    return min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3])


README = """GroomLab press kit
==================

The GroomLab logo, for articles, videos, tutorials and tools that work with GroomLab.
More at https://getgroomlab.com/press. Questions: support@getgroomlab.com.

Writing the name
----------------
Write it GroomLab: one word, capital G and capital L, in running text, headlines and titles.

  Do:     GroomLab
  Don't:  Groom Lab, Groomlab, groomLab, GROOMLAB, Groom-Lab, GL

- Possessive: GroomLab's.
- Lowercase "groomlab" only where a system needs it: URLs, package and file names, handles
  (getgroomlab.com, github.com/thekesslereffect/groomlab).
- Product names: GroomLab for Maya, GroomLab for Blender, the GroomLab app.

Using the logo
--------------
- Use the files as they are. Don't recolor them beyond the versions here, stretch or squash them,
  add effects (shadows, outlines, gradients), or rearrange, crop or redraw the mark and the name.
- Keep clear space around the logo: at least half the mark's height on every side.
- Color versions on white or light backgrounds; the color-on-dark lockup or the white versions on dark
  backgrounds and photos; black where only one dark color prints.
- When you mention GroomLab, link to https://getgroomlab.com.
- Describe your work as "works with GroomLab" or "Hair from GroomLab". Don't present it as official,
  as a partnership, or as endorsed by GroomLab.

Files
-----
groomlab-mark-*        the mark (three strands), square
groomlab-wordmark-*    the name
groomlab-lockup-*      the mark and the name together (the main logo)
  -color               brand colors, name in ink, for light backgrounds
  -color-on-dark       (lockup only) brand colors, name in white, for dark backgrounds
  -black, -white       one color
Each as .svg (vector, any size) and .png 512 and 2048 px wide, transparent.

Colors
------
Red     #FF4D4F
Yellow  #FFC53D
Blue    #2F7BF6
Ink     #1D1D1F  (the name on light backgrounds)
Night   #1C1D21  (dark backgrounds)

Trademarks
----------
GroomLab's source code is open source under the Apache License 2.0, which doesn't grant rights to
the GroomLab name or logo (section 6). GroomLab and the GroomLab logo are trademarks of
Nick Rutlinh; these files may be used to refer to GroomLab as described above.
"""


def rasterize(jobs):
    sharp = os.path.join(WEB, "node_modules", "sharp")
    if not os.path.isdir(sharp):
        sys.exit("sharp not found: run `npm ci` in web/ first")
    js = r"""
const sharp = require(process.argv[1]);
const fs = require('fs');
const jobs = JSON.parse(fs.readFileSync(0, 'utf8'));
(async () => {
  for (const j of jobs) {
    const svg = fs.readFileSync(j.svg);
    const width = parseFloat(/width="([\d.]+)"/.exec(svg.toString())[1]);
    for (const w of j.widths) {
      await sharp(svg, { density: (72 * w) / width })
        .resize({ width: w })
        .png({ compressionLevel: 9, adaptiveFiltering: true })
        .toFile(j.out.replace('{w}', w));
    }
  }
})().catch((e) => { console.error(e); process.exit(1); });
"""
    subprocess.run(["node", "-e", js, sharp], input=json.dumps(jobs), text=True, check=True)


def main():
    global WEIGHT, OUT
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--font", help="the Inter variable font (woff2 or ttf); default: the site's build output")
    ap.add_argument("--weight", type=int, default=WEIGHT, help="the wordmark's Inter weight (default %d)" % WEIGHT)
    ap.add_argument("--out", default=OUT, help="the folder to write (default web/public/press)")
    args = ap.parse_args()
    WEIGHT, OUT = args.weight, os.path.abspath(args.out)
    os.makedirs(OUT, exist_ok=True)

    strands = read_mark()
    mbox = mark_bounds(strands)
    font = find_font(args.font)
    # The wordmark alone, and beside the mark as in the header (the mark's box is x 0..30).
    word_d, word_b = wordmark(font, 0.0)
    lock_d, lock_b = wordmark(font, 30.0 + GAP)

    pieces = {
        "mark": ((0.0, 0.0, 30.0, 30.0), lambda mark, text: strands_svg(strands, mark)),
        "wordmark": (pad(word_b), lambda mark, text: text_svg(word_d, text)),
        "lockup": (pad(union(mbox, lock_b)), lambda mark, text: strands_svg(strands, mark) + text_svg(lock_d, text)),
    }
    versions = {
        "color": (None, INK),
        "color-on-dark": (None, WHITE),
        "black": (BLACK, BLACK),
        "white": (WHITE, WHITE),
    }

    os.makedirs(OUT, exist_ok=True)
    for old in glob.glob(os.path.join(OUT, "groomlab-*")):
        os.remove(old)
    files, jobs = [], []
    for piece, (box, body) in pieces.items():
        for version, (mark, text) in versions.items():
            if version == "color-on-dark" and piece != "lockup":
                continue  # the mark's colors work on both; the wordmark on dark is the white one
            stem = f"groomlab-{piece}-{version}"
            label = "GroomLab logo" if piece != "wordmark" else "GroomLab"
            with open(os.path.join(OUT, stem + ".svg"), "w", encoding="utf-8", newline="\n") as f:
                f.write(svg_doc(box, body(mark, text), label))
            jobs.append({"svg": os.path.join(OUT, stem + ".svg"), "out": os.path.join(OUT, stem + "-{w}.png"), "widths": WIDTHS})
            files += [stem + ".svg"] + [f"{stem}-{w}.png" for w in WIDTHS]
    rasterize(jobs)

    zpath = os.path.join(OUT, "groomlab-press-kit.zip")
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        stamp = (2026, 1, 1, 0, 0, 0)
        info = zipfile.ZipInfo("groomlab-press-kit/README.txt", stamp)
        info.compress_type = zipfile.ZIP_DEFLATED
        z.writestr(info, README.replace("\n", "\r\n"))
        for name in files:
            info = zipfile.ZipInfo(f"groomlab-press-kit/{name}", stamp)
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(os.path.join(OUT, name), "rb") as f:
                z.writestr(info, f.read())

    total = sum(os.path.getsize(os.path.join(OUT, n)) for n in files + ["groomlab-press-kit.zip"])
    print(f"font {os.path.relpath(font, ROOT)}")
    print(f"{len(files)} files + zip in {os.path.relpath(OUT, ROOT)}: {total / 1e6:.2f} MB")


if __name__ == "__main__":
    main()

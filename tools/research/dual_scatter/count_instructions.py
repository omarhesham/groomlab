# SPDX-License-Identifier: Apache-2.0
"""Compiles shade_variants.hlsl per variant with fxc (ps_5_0, /O3) and counts its instructions:
the per-point shading cost of each dual-scattering variant (docs/research/dual_scatter.md).

    python tools/research/dual_scatter/count_instructions.py
"""

import glob
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def fxc():
    found = sorted(glob.glob(r"C:\Program Files (x86)\Windows Kits\10\bin\10.*\x64\fxc.exe"))
    if not found:
        raise SystemExit("fxc.exe not found (Windows SDK)")
    return found[-1]


def count(variant, lights):
    with tempfile.TemporaryDirectory() as tmp:
        listing = os.path.join(tmp, "out.asm")
        subprocess.run([fxc(), "/nologo", "/T", "ps_5_0", "/O3", "/E", "main", "/D", "VARIANT=%d" % variant, "/D", "LIGHTS=%d" % lights,
                        "/Fc", listing, os.path.join(HERE, "shade_variants.hlsl")], check=True, capture_output=True)
        text = open(listing).read()
    m = re.search(r"approximately (\d+) instruction slots used", text, re.I)
    slots = int(m.group(1)) if m else -1
    # The instructions: listing lines "  NN: op ..." or "op ..." past the declarations.
    ops = []
    for line in text.splitlines():
        body = line.split("//")[0].strip()
        if not body or body.startswith(("dcl", "ps_", "vs_", "cs_")):
            continue
        op = body.split()[0]
        if re.match(r"^[a-z_]+(\.[a-z0-9_]+)?$", op):
            ops.append(op)
    transcendental = sum(1 for o in ops if o.split("_")[0] in ("exp", "log", "sqrt", "rsq", "rcp", "sincos"))
    return (slots if slots > 0 else len(ops)), transcendental


def main():
    names = {0: "V0 as shipped", 1: "V1 factorized (exact)", 2: "V2 azimuth-averaged"}
    for lights in (1, 11):
        for v in (0, 1, 2):
            slots, trans = count(v, lights)
            print("%-24s lights %2d: %5d instruction slots, %4d transcendental (exp/log/sqrt/rsq/rcp/sincos)" % (names[v], lights, slots, trans))


if __name__ == "__main__":
    main()

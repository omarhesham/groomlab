# SPDX-License-Identifier: Apache-2.0
"""The app's real hair shader (hosts/app/shaders/hair_lookdev.frag) through the real toolchain: qsb to
HLSL 5.0 (what the Direct3D 11 backend compiles), then fxc /O3: instruction slots as shipped, with
the multiple-scattering spread lobes removed, and with the whole dual-scattering branch removed, for
the per-point pass (HAIR_POINT_SHADE) and the per-fragment one. What the global multiple scattering
costs today bounds what any closed form of it can save (docs/research/dual_scatter.md).

    python tools/research/dual_scatter/count_real_shader.py
"""

import glob
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SHADER = os.path.join(ROOT, "hosts", "app", "shaders", "hair_lookdev.frag")
QSB = os.path.expanduser(r"~\groomlab-deps\Qt\6.8.3\msvc2022_64\bin\qsb.exe")


def fxc():
    return sorted(glob.glob(r"C:\Program Files (x86)\Windows Kits\10\bin\10.*\x64\fxc.exe"))[-1]


SPREAD = "vec3 spread = lobes(T, L, V, color, B, s, shift, ior, spreadSq);"
BRANCH_START = "    if (count > 0.05) {"


PAIR = """
// lobes() for two longitudinal spreads at once (0 and spreadSq): everything but the three
// Gaussians made once (the exact factorization).
void lobesPair(vec3 T, vec3 L, vec3 V, vec3 color, float B, float s, float shift, float ior, float spreadSq,
               out vec3 single, out vec3 spreadOut) {
    float sinL = clamp(dot(T, L), -1.0, 1.0), sinV = clamp(dot(T, V), -1.0, 1.0);
    float cosThetaD = max(cos(0.5 * abs(asin(sinV) - asin(sinL))), 0.05);
    vec3 Lp = L - sinL * T, Vp = V - sinV * T;
    float cosPhi = dot(Lp, Vp) * inversesqrt(max(dot(Lp, Lp) * dot(Vp, Vp), 1e-8));
    float cosHalfPhi = sqrt(clamp(0.5 + 0.5 * cosPhi, 0.0, 1.0));
    float sinThetaH = 0.5 * (sinL + sinV);
    float Nr = 0.25 * cosHalfPhi;
    float Fr = fresnel(ior, sqrt(clamp(0.5 + 0.5 * dot(V, L), 0.0, 1.0)));
    vec3 XR = (Nr * Fr) * specR.rgb * specularTint;
    float sinThetaD = sqrt(max(1.0 - cosThetaD * cosThetaD, 0.0));
    float nPrime = sqrt(max(ior * ior - sinThetaD * sinThetaD, 1.0)) / cosThetaD;
    float a = 1.0 / nPrime;
    float h = cosHalfPhi * (1.0 + a * (0.6 - 0.8 * cosPhi));
    float f = fresnel(ior, cosThetaD * sqrt(clamp(1.0 - h * h, 0.0, 1.0)));
    vec3 Ttt = pow(color, vec3(2.0 * sqrt(max(1.0 - h * h * a * a, 0.0)) / cosThetaD));
    float kTT = 1.0 / (1.0 / 3.65 + s * s);
    float Ntt = 0.945 * sqrt(kTT / (2.0 * PI)) * exp(kTT * (-cosPhi - 1.0));
    vec3 XTT = (Ntt * sq(1.0 - f)) * Ttt * specTT.rgb;
    float f2 = fresnel(ior, cosThetaD * 0.5);
    vec3 Ttrt = pow(color, vec3(3.5 / cosThetaD));
    float kTRT = 1.0 / (1.0 / 17.0 + s * s);
    float Ntrt = 0.758 * sqrt(kTRT / (2.0 * PI)) * exp(kTRT * (cosPhi - 1.0));
    vec3 XTRT = (Ntrt * f2 * sq(1.0 - f2)) * Ttrt * specTRT.rgb;
    single = gauss(B, sinThetaH + shift) * XR + gauss(0.5 * B, sinThetaH - 0.5 * shift) * XTT + gauss(2.0 * B, sinThetaH - 1.5 * shift) * XTRT;
    spreadOut = gauss(sqrt(B * B + spreadSq), sinThetaH + shift) * XR + gauss(sqrt(0.25 * B * B + spreadSq), sinThetaH - 0.5 * shift) * XTT +
                gauss(sqrt(4.0 * B * B + spreadSq), sinThetaH - 1.5 * shift) * XTRT;
}
"""


def variant(text, name):
    if name == "shipped":
        return text
    if name == "factorized (exact)":
        anchor = "// One light: the single scattering"
        out = text.replace(anchor, PAIR + "\n" + anchor, 1)
        out = out.replace("    vec3 single = lobes(T, L, V, color, B, s, shift, ior, 0.0);",
                          "    vec3 single, spreadPair;\n    lobesPair(T, L, V, color, B, s, shift, ior, spreadSq, single, spreadPair);", 1)
        assert SPREAD in out
        return out.replace(SPREAD, "vec3 spread = spreadPair;")
    if name == "no spread lobes":
        assert SPREAD in text
        return text.replace(SPREAD, "vec3 spread = vec3(0.0);")
    if name == "no multiple scattering":
        i = text.index(BRANCH_START)
        j = text.index("    }\n", i) + len("    }\n")
        return text[:i] + text[j:]
    if name == "3 lights (no environment lights)":
        return text.replace("for (int i = 0; i < 8; ++i) {\n        if (envDir[i].w < 0.5) continue;", "for (int i = 0; i < 0; ++i) {\n        if (envDir[i].w < 0.5) continue;")
    raise ValueError(name)


def slots(source, defines):
    with tempfile.TemporaryDirectory() as tmp:
        frag = os.path.join(tmp, "hair.frag")
        with open(frag, "w") as f:
            f.write(source)
        pack = os.path.join(tmp, "hair.qsb")
        cmd = [QSB, "--hlsl", "50", "-o", pack, frag]
        for d in defines:
            cmd += ["-D", d]
        subprocess.run(cmd, check=True, capture_output=True)
        hlsl = os.path.join(tmp, "hair.hlsl")
        subprocess.run([QSB, "-x", "hlsl,50", "-o", hlsl, pack], check=True, capture_output=True)
        listing = os.path.join(tmp, "hair.asm")
        subprocess.run([fxc(), "/nologo", "/T", "ps_5_0", "/O3", "/E", "main", "/Fc", listing, hlsl], check=True, capture_output=True)
        text = open(listing).read()
    m = re.search(r"approximately (\d+) instruction slots used", text, re.I)
    if m:
        return int(m.group(1))
    return sum(1 for l in text.splitlines() if re.match(r"^\s*\d+:\s", l) or re.match(r"^\s{2}[a-z_]+(\.[a-z0-9_]+)?\s", l))


def main():
    text = open(SHADER).read()
    for label, defines in (("per point (HAIR_POINT_SHADE)", ["HAIR_POINT_SHADE"]), ("per fragment", [])):
        base = None
        for name in ("shipped", "factorized (exact)", "no spread lobes", "no multiple scattering", "3 lights (no environment lights)"):
            n = slots(variant(text, name), defines)
            base = base or n
            print("%-30s %-34s %5d instruction slots (%+.1f%%)" % (label, name, n, 100.0 * (n - base) / base))


if __name__ == "__main__":
    main()

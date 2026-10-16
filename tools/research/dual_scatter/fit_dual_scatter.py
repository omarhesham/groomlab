# SPDX-License-Identifier: Apache-2.0
"""Research prototype (docs/research/dual_scatter.md): cheaper closed forms of the viewport's dual
scattering (hosts/app/shaders/hair_lookdev.frag shadeLight), measured against it.

    python tools/research/dual_scatter/fit_dual_scatter.py [--out results.json]

The shader's shadeLight is ported to numpy line for line (float64) and evaluated over random
light/view directions around a strand, across melanin, roughness and the number of hairs in the
way (the deep opacity map's count). Variants:

  V0  the shader as it is: lobes() evaluated twice when hairs are in the way (single, and the
      spread lobes of the forward-scattered light).
  V1  exact factorization: the spread lobes share everything with the single ones but their three
      longitudinal Gaussians (the azimuthal terms, Fresnel and attenuations depend on the angles and
      the color only), so lobes() runs once and the spread costs three Gaussians.
  V2  azimuth-averaged global term (Zinke's global multiple scattering is azimuthally broad): the
      spread lobes replaced by A(color) G(sigma_n, sin theta_h - mu): one Gaussian a light, A a fitted
      closed form of the fiber color (per channel), mu and sigma from the lobes' moments.

Reports relative errors of the per-light radiance and of the multiple-scattering part, and writes the
fitted constants.
"""

import argparse
import json
import math

import numpy as np

PI = math.pi


def gauss(b, x):
    return np.exp(-0.5 * x * x / (b * b)) / (2.50662827 * b)


def fresnel(ior, c):
    f0 = ((ior - 1.0) / (ior + 1.0)) ** 2
    return f0 + (1.0 - f0) * np.power(1.0 - np.clip(c, 0.0, 1.0), 5.0)


def angles(T, L, V):
    sinL = np.clip(np.sum(T * L, -1), -1, 1)
    sinV = np.clip(np.sum(T * V, -1), -1, 1)
    cosThetaD = np.maximum(np.cos(0.5 * np.abs(np.arcsin(sinV) - np.arcsin(sinL))), 0.05)
    Lp = L - sinL[..., None] * T
    Vp = V - sinV[..., None] * T
    cosPhi = np.sum(Lp * Vp, -1) / np.sqrt(np.maximum(np.sum(Lp * Lp, -1) * np.sum(Vp * Vp, -1), 1e-8))
    cosHalfPhi = np.sqrt(np.clip(0.5 + 0.5 * cosPhi, 0, 1))
    sinThetaH = 0.5 * (sinL + sinV)
    return sinL, sinV, cosThetaD, cosPhi, cosHalfPhi, sinThetaH


def lobe_parts(T, L, V, color, B, s, shift, ior, spec):
    """The three lobes' longitudinal centers and widths and their azimuthal x Fresnel x attenuation
    factors X_p (RGB), as lobes() computes them (spec: R, TT, TRT weights as the shader's uniforms)."""
    sinL, sinV, cosThetaD, cosPhi, cosHalfPhi, sinThetaH = angles(T, L, V)
    Nr = 0.25 * cosHalfPhi
    Fr = fresnel(ior, np.sqrt(np.clip(0.5 + 0.5 * np.sum(V * L, -1), 0, 1)))
    XR = (Nr * Fr)[..., None] * spec[0]
    sinThetaD = np.sqrt(np.maximum(1.0 - cosThetaD * cosThetaD, 0))
    nPrime = np.sqrt(np.maximum(ior * ior - sinThetaD * sinThetaD, 1.0)) / cosThetaD
    a = 1.0 / nPrime
    h = cosHalfPhi * (1.0 + a * (0.6 - 0.8 * cosPhi))
    f = fresnel(ior, cosThetaD * np.sqrt(np.clip(1.0 - h * h, 0, 1)))
    Ttt = np.power(color[None, :], (2.0 * np.sqrt(np.maximum(1.0 - h * h * a * a, 0)) / cosThetaD)[..., None])
    kTT = 1.0 / (1.0 / 3.65 + s * s)
    Ntt = 0.945 * math.sqrt(kTT / (2 * PI)) * np.exp(kTT * (-cosPhi - 1.0))
    XTT = ((Ntt * (1.0 - f) ** 2)[..., None]) * Ttt * spec[1]
    f2 = fresnel(ior, cosThetaD * 0.5)
    Ttrt = np.power(color[None, :], (3.5 / cosThetaD)[..., None])
    kTRT = 1.0 / (1.0 / 17.0 + s * s)
    Ntrt = 0.758 * math.sqrt(kTRT / (2 * PI)) * np.exp(kTRT * (cosPhi - 1.0))
    XTRT = ((Ntrt * f2 * (1.0 - f2) ** 2)[..., None]) * Ttrt * spec[2]
    centers = np.array([-shift, 0.5 * shift, 1.5 * shift])  # sinThetaH - c: gauss(b, sinThetaH - c)
    widths2 = np.array([B * B, 0.25 * B * B, 4.0 * B * B])
    return sinL, sinThetaH, [XR, XTT, XTRT], centers, widths2


def lobes_from_parts(sinThetaH, X, centers, widths2, spreadSq):
    out = 0.0
    for p in range(3):
        M = gauss(np.sqrt(widths2[p] + spreadSq), sinThetaH + (-centers[p] if p == 0 else -centers[p]))
        out = out + M[..., None] * X[p]
    return out


def shade_light(T, L, V, color, count, B, s, shift, ior, ms, density=1.0, spec=(1, 1, 1), variant="V0", fit=None):
    """shadeLight; returns (result, multiple-scattering part) RGB."""
    sinL, sinThetaH, X, centers, widths2 = lobe_parts(T, L, V, color, B, s, shift, ior, spec)
    # (the shader's R lobe: gauss(bR, sinThetaH + shift): center -shift; TT: sinThetaH - 0.5 shift; TRT: -1.5 shift)
    af = 0.04 + 0.86 * np.power(color, 1.2)
    ab = 0.02 + 0.3 * np.power(color, 0.85)
    direct = np.exp(-count * density)
    Tf = (0.7 * np.power(af, count) if count > 0 else np.zeros(3)) * ms
    bTT = 0.5 * B
    spreadSq = count * bTT * bTT
    afL = af @ np.array([0.2126, 0.7152, 0.0722])
    Ab = ab * (afL * afL)
    sigmaB = math.sqrt(4 * B * B + bTT * bTT)
    gb = gauss(sigmaB, sinThetaH - 2.0 * shift)
    gbSpread = gauss(math.sqrt(sigmaB * sigmaB + spreadSq), sinThetaH - 2.0 * shift)
    back = 2.0 * Ab / PI * ms
    single = lobes_from_parts(sinThetaH, X, centers, widths2, 0.0)
    result = direct * (single + 0.7 * back[None, :] * gb[..., None])
    msPart = np.zeros_like(result)
    if count > 0.05:
        if variant in ("V0", "V1"):
            spread = lobes_from_parts(sinThetaH, X, centers, widths2, spreadSq)
        else:  # V2: azimuth-averaged closed form
            A = fit_amplitude(color, fit)  # RGB
            mu = fit["mu"] * shift
            sig = math.sqrt(fit["w2"] * B * B + spreadSq)
            spread = gauss(sig, sinThetaH - mu)[..., None] * A[None, :]
        msPart = Tf[None, :] * (spread + PI * 0.7 * back[None, :] * gbSpread[..., None])
        result = result + msPart
    w = np.sqrt(np.maximum(1.0 - sinL * sinL, 0))[..., None]
    return result * w, msPart * w


def fit_amplitude(color, fit):
    # A(c) = a0 + a1 c^k1 + a2 c^k2 per channel (the spread lobes' azimuthal mean, per channel)
    c = np.asarray(color)
    return fit["a0"] + fit["a1"] * np.power(c, fit["k1"]) + fit["a2"] * np.power(c, fit["k2"])


def melanin_color(melanin, redness=0.15, scale=5.0):
    eu = np.array([0.419, 0.697, 1.37])
    pheo = np.array([0.187, 0.4, 1.05])
    return np.clip(np.exp(-scale * melanin * ((1 - redness) * eu + redness * pheo)), 1e-4, 1.0)


def longitudinal_width(r):
    r = min(max(r, 0.02), 1.0)
    s = 0.726 * r + 0.812 * r * r + 3.7 * r ** 20
    return min(max(0.7071 * s, 0.02), 1.5)


def azimuthal_spread(r):
    r = min(max(r, 0.0), 1.0)
    return 0.265 * r + 1.194 * r * r + 5.372 * r ** 22


def random_dirs(n, rng):
    v = rng.normal(size=(n, 3))
    return v / np.linalg.norm(v, axis=1, keepdims=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="")
    ap.add_argument("--samples", type=int, default=20000)
    args = ap.parse_args()
    rng = np.random.default_rng(7)
    n = args.samples
    T = np.tile(np.array([0.0, 1.0, 0.0]), (n, 1))
    L = random_dirs(n, rng)
    V = random_dirs(n, rng)
    ior, shift, ms = 1.55, 3.0 * PI / 180.0, 1.0

    # ---- Fit V2's constants: the azimuthal mean of the spread lobes' X_p sum, per channel value ----
    # For each channel value c, A(c) = mean over directions of sum_p X_p (the spread lobes'
    # azimuthal x Fresnel x attenuation sum); mu, w2: the lobes' luminance-weighted center and width.
    cs = np.linspace(0.0005, 1.0, 60)
    amps, mus, w2s = [], [], []
    for r in (0.15, 0.25, 0.4):
        B, s = longitudinal_width(r), azimuthal_spread(r)
        for c in cs:
            col = np.array([c, c, c])
            _, _, X, centers, widths2 = lobe_parts(T, L, V, col, B, s, shift, ior, (1, 1, 1))
            sums = [x[:, 0].mean() for x in X]
            amps.append((c, r, sum(sums)))
            wts = np.array(sums) / max(sum(sums), 1e-12)
            mu = float(np.sum(wts * centers)) / shift  # (as sinThetaH - mu)
            mus.append(mu)
            w2s.append(float(np.sum(wts * (widths2 + (centers - mu * shift) ** 2))) / (B * B))
    amps = np.array(amps)
    # Least squares for a0, a1, a2 over a grid of exponents k1, k2.
    best = None
    for k1 in np.linspace(0.5, 3.0, 26):
        for k2 in np.linspace(3.0, 9.0, 25):
            Amat = np.stack([np.ones(len(amps)), amps[:, 0] ** k1, amps[:, 0] ** k2], 1)
            coef, *_ = np.linalg.lstsq(Amat, amps[:, 2], rcond=None)
            err = np.abs(Amat @ coef - amps[:, 2]).max()
            if best is None or err < best[0]:
                best = (err, k1, k2, coef)
    fit = {"a0": float(best[3][0]), "a1": float(best[3][1]), "a2": float(best[3][2]), "k1": float(best[1]), "k2": float(best[2]),
           "mu": float(np.median(mus)), "w2": float(np.median(w2s)), "ampFitMaxErr": float(best[0])}
    print("V2 fit: A(c) = %.5f + %.5f c^%.2f + %.5f c^%.2f (max abs err %.4f over the grid); mu = %.3f shift, width^2 = %.3f B^2"
          % (fit["a0"], fit["a1"], fit["k1"], fit["a2"], fit["k2"], fit["ampFitMaxErr"], fit["mu"], fit["w2"]))

    # ---- Error tables ----
    lumw = np.array([0.2126, 0.7152, 0.0722])
    table = []
    print("\nmelanin rough count | V1 rel err (result) | V2 rel err result mean/p95, of the MS part mean/p95 | MS share of result")
    for melanin in (0.05, 0.2, 0.35, 0.5, 0.7, 0.9):
        col = melanin_color(melanin)
        for rough in (0.1, 0.2, 0.3, 0.45, 0.6):
            B, s = longitudinal_width(rough), azimuthal_spread(rough)
            for count in (0.5, 1, 2, 4, 8, 16):
                r0, m0 = shade_light(T, L, V, col, count, B, s, shift, ior, ms, variant="V0")
                r1, _ = shade_light(T, L, V, col, count, B, s, shift, ior, ms, variant="V1")
                r2, m2 = shade_light(T, L, V, col, count, B, s, shift, ior, ms, variant="V2", fit=fit)
                y0, y1, y2 = r0 @ lumw, r1 @ lumw, r2 @ lumw
                ym0, ym2 = m0 @ lumw, m2 @ lumw
                scale = max(y0.mean(), 1e-12)
                e1 = np.abs(y1 - y0).max() / scale
                e2 = np.abs(y2 - y0) / scale
                em = np.abs(ym2 - ym0) / max(ym0.mean(), 1e-12)
                # Color: the mean RGB's chromaticity error (CIE-ish: relative per channel of the mean)
                c0, c2 = r0.mean(0), r2.mean(0)
                chroma = float(np.abs(c2 / max(c2.sum(), 1e-12) - c0 / max(c0.sum(), 1e-12)).max())
                row = {"melanin": melanin, "roughness": rough, "count": count, "v1MaxRel": float(e1), "v2MeanRel": float(e2.mean()),
                       "v2P95Rel": float(np.quantile(e2, 0.95)), "v2MsMeanRel": float(em.mean()), "v2MsP95Rel": float(np.quantile(em, 0.95)),
                       "msShare": float(ym0.mean() / scale), "v2MeanBias": float((y2.mean() - y0.mean()) / scale), "v2Chroma": chroma}
                table.append(row)
                if rough in (0.2, 0.45) and count in (1, 4, 16):
                    print("%.2f    %.2f  %4.1f  | %.1e | %.3f / %.3f, MS %.3f / %.3f (bias %+.3f, chroma %.4f) | %.2f"
                          % (melanin, rough, count, e1, row["v2MeanRel"], row["v2P95Rel"], row["v2MsMeanRel"], row["v2MsP95Rel"],
                             row["v2MeanBias"], chroma, row["msShare"]))
    allv2 = np.array([r["v2MeanRel"] for r in table])
    allp95 = np.array([r["v2P95Rel"] for r in table])
    bias = np.array([r["v2MeanBias"] for r in table])
    print("\nV1 (exact factorization): largest relative difference over the table %.2e" % max(r["v1MaxRel"] for r in table))
    print("V2 over the table: mean relative error of a light's radiance %.3f (worst cell %.3f), p95 %.3f (worst %.3f); mean bias %+.3f (worst %+.3f)"
          % (allv2.mean(), allv2.max(), allp95.mean(), allp95.max(), bias.mean(), bias[np.abs(bias).argmax()]))
    if args.out:
        with open(args.out, "w") as f:
            json.dump({"fit": fit, "table": table}, f, indent=1)


if __name__ == "__main__":
    main()

// SPDX-License-Identifier: Apache-2.0
/*
 * A tiny 2D cousin of the groom graph, for the landing page's viewports: Scatter places roots on a
 * scalp arc, Interpolate grows each strand from the guides either side of it, then Clump, Curl and
 * Noise displace it, in that order, and HairColor picks its color. Plain canvas, no dependencies:
 * a few hundred strands of 18 points each, cheap enough to redraw every frame.
 */

export interface HairColor {
  /** 0 blonde to 1 black, as HairColor's `melanin`. */
  melanin: number;
  /** 0 to 1, as HairColor's `redness`. */
  redness: number;
}

export interface StrandParams {
  /** Strand count (Scatter's density, scaled to the viewport). */
  density: number;
  /** Clump `strength`, 0 to 1; 0 when bypassed. */
  clump: number;
  /** Clump `count`. */
  clumpCount: number;
  /** Curl `radius`, 0 to 1; 0 when bypassed. */
  curl: number;
  /** Curl `frequency`, turns along the strand. */
  curlFrequency: number;
  /** Noise `amplitude`, 0 to 1; 0 when bypassed. */
  noise: number;
  /** 'mark' paints the logo's primaries; null (bypassed) leaves the strands graphite. */
  color: HairColor | 'mark' | null;
  showGuides: boolean;
  /** Seed for the root layout. */
  seed: number;
}

export const defaultParams: StrandParams = {
  density: 320,
  clump: 0.62,
  clumpCount: 16,
  curl: 0.45,
  curlFrequency: 3,
  noise: 0.2,
  color: { melanin: 0.55, redness: 0.45 },
  showGuides: false,
  seed: 7,
};

const POINTS = 18;
const GUIDES = 9;
const MAX_STRANDS = 900;

function rng(seed: number) {
  let s = seed >>> 0 || 1;
  return () => {
    s ^= s << 13;
    s ^= s >>> 17;
    s ^= s << 5;
    return (s >>> 0) / 4294967296;
  };
}

/** Per-strand random values, fixed for a seed so sliders never reshuffle the hair. */
interface Seeds {
  u: Float32Array;
  len: Float32Array;
  bend: Float32Array;
  phase: Float32Array;
  n1: Float32Array;
  n2: Float32Array;
  shade: Float32Array;
  guideLen: Float32Array;
  guideBend: Float32Array;
  clumpU: Float32Array;
  clumpPhase: Float32Array;
}

function makeSeeds(seed: number): Seeds {
  const r = rng(seed * 9973 + 17);
  const f = (n: number, g: () => number) => Float32Array.from({ length: n }, g);
  // Roots: stratified so any density covers the scalp evenly (the first n of them are used).
  const u = new Float32Array(MAX_STRANDS);
  for (let i = 0; i < MAX_STRANDS; i++) u[i] = r();
  return {
    u,
    len: f(MAX_STRANDS, () => 0.82 + r() * 0.3),
    bend: f(MAX_STRANDS, () => (r() - 0.5) * 0.22),
    phase: f(MAX_STRANDS, () => r() * Math.PI * 2),
    n1: f(MAX_STRANDS, () => r() * Math.PI * 2),
    n2: f(MAX_STRANDS, () => r() * Math.PI * 2),
    shade: f(MAX_STRANDS, () => r()),
    guideLen: f(GUIDES, () => 0.7 + r() * 0.3),
    guideBend: f(GUIDES, () => 0.55 + r() * 0.45),
    clumpU: f(64, () => r()),
    clumpPhase: f(64, () => r() * Math.PI * 2),
  };
}

const graphite = ['#3a3a3f', '#55555b', '#6e6e74'];
const mark = ['#FF4D4F', '#FFC53D', '#2F7BF6', '#FF4D4F', '#2F7BF6'];

function mix(a: number[], b: number[], t: number) {
  return a.map((v, i) => v + (b[i] - v) * t);
}
const hex = (c: number[]) => `rgb(${c.map((v) => Math.round(Math.max(0, Math.min(255, v)))).join(',')})`;

/** Four shades of a melanin/redness hair color (melaninRandom-ish), root to tip. */
function hairShades({ melanin, redness }: HairColor): [string, string][] {
  const blonde = [226, 196, 140];
  const brown = [86, 52, 30];
  const black = [24, 20, 20];
  const ginger = [178, 72, 28];
  return [0.82, 0.92, 1, 1.1].map((k) => {
    const m = Math.min(1, melanin * k);
    let c = m < 0.6 ? mix(blonde, brown, m / 0.6) : mix(brown, black, (m - 0.6) / 0.4);
    c = mix(c, ginger, redness * (1 - m * 0.7) * 0.85);
    return [hex(mix(c, [0, 0, 0], 0.25)), hex(mix(c, [255, 255, 255], 0.12))];
  });
}

export interface Frame {
  width: number;
  height: number;
  /** Seconds, for the sway. */
  time: number;
  /** 0 to 1: how much of each strand has grown in. */
  grow: number;
  sway: boolean;
}

export class StrandRenderer {
  private seeds: Seeds;
  private seed: number;
  private xs = new Float32Array(MAX_STRANDS * POINTS);
  private ys = new Float32Array(MAX_STRANDS * POINTS);
  private gradients = new Map<string, CanvasGradient>();
  private gradientKey = '';

  constructor(seed = 7) {
    this.seed = seed;
    this.seeds = makeSeeds(seed);
  }

  /** Draws one frame of the groom into a context already scaled to CSS pixels. */
  draw(ctx: CanvasRenderingContext2D, p: StrandParams, f: Frame) {
    if (p.seed !== this.seed) {
      this.seed = p.seed;
      this.seeds = makeSeeds(p.seed);
    }
    const S = this.seeds;
    const { width: W, height: H } = f;
    ctx.clearRect(0, 0, W, H);

    // The scalp: a wide arc along the bottom.
    const R = Math.max(W * 0.95, H * 0.9);
    const cx = W / 2;
    const top = H * 0.88;
    const cy = top + R;
    const span = Math.asin(Math.min(0.95, (W * 0.43) / R));
    const unit = Math.min(W, H * 1.25);
    const baseLen = H * 0.74;

    // A strand's shape, from the guides either side of its root.
    const shape = (u: number, lenK: number, bendK: number) => {
      const gu = u * (GUIDES - 1);
      const g0 = Math.max(0, Math.min(GUIDES - 2, Math.floor(gu)));
      const w = gu - g0;
      const L = (S.guideLen[g0] * (1 - w) + S.guideLen[g0 + 1] * w) * lenK * baseLen;
      const bend = (S.guideBend[g0] * (1 - w) + S.guideBend[g0 + 1] * w) + bendK;
      return { L, bend };
    };

    const n = Math.min(MAX_STRANDS, Math.round(p.density));
    const sway = f.sway ? 1 : 0;
    const K = Math.max(2, Math.min(64, Math.round(p.clumpCount)));
    const curlR = p.curl * unit * 0.024;
    const noiseA = p.noise * unit * 0.045;

    // Clump centers: strands of their own, grown the same way.
    const cxs = new Float32Array(K * POINTS);
    const cys = new Float32Array(K * POINTS);
    const centerU = new Float32Array(K);
    for (let k = 0; k < K; k++) {
      centerU[k] = (k + 0.2 + S.clumpU[k] * 0.6) / K;
      this.grow(centerU[k], shape(centerU[k], 1.04, 0), cx, cy, R, span, f, sway, cxs, cys, k);
    }

    const xs = this.xs;
    const ys = this.ys;
    for (let i = 0; i < n; i++) {
      // Stratified roots: strand i of n sits in its own slice, jittered.
      const u = (i + S.u[i]) / n;
      const sh = shape(u, S.len[i], S.bend[i]);
      this.grow(u, sh, cx, cy, R, span, f, sway, xs, ys, i);
      const k = Math.min(K - 1, Math.max(0, Math.round(u * K - 0.5)));
      const ph = p.clump > 0 ? S.clumpPhase[k] * 0.7 + S.phase[i] * 0.3 : S.phase[i];
      const o = i * POINTS;
      const c = k * POINTS;
      for (let j = 1; j < POINTS; j++) {
        const s = j / (POINTS - 1);
        let x = xs[o + j];
        let y = ys[o + j];
        // Clump: pull toward the center strand, more toward the tip.
        if (p.clump > 0) {
          const t = p.clump * Math.pow(s, 0.7) * 0.92;
          x += (cxs[c + j] - x) * t;
          y += (cys[c + j] - y) * t;
        }
        // The strand's direction here, for Curl and Noise to push across it.
        const dx = xs[o + j] - xs[o + j - 1];
        const dy = ys[o + j] - ys[o + j - 1];
        const dl = Math.hypot(dx, dy) || 1;
        const nx = -dy / dl;
        const ny = dx / dl;
        let off = 0;
        if (curlR > 0) off += curlR * Math.min(1, s * 3) * Math.sin(Math.PI * 2 * p.curlFrequency * s + ph);
        if (noiseA > 0)
          off += noiseA * Math.pow(s, 1.3) * (Math.sin(s * 5.3 + S.n1[i]) + 0.55 * Math.sin(s * 13.1 + S.n2[i]));
        xs[o + j] = x + nx * off;
        ys[o + j] = y + ny * off;
      }
    }

    // Colors: a few buckets, each one path with a root-to-tip gradient.
    const shades: [string, string][] =
      p.color === 'mark'
        ? mark.map((m) => [m, m])
        : p.color
          ? hairShades(p.color)
          : graphite.map((g) => [g, g]);
    const key = `${W}x${H}:${shades.flat().join()}`;
    if (key !== this.gradientKey) {
      this.gradients.clear();
      this.gradientKey = key;
    }
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.lineWidth = Math.max(1, unit / 420);
    ctx.globalAlpha = p.color === 'mark' ? 0.92 : 0.88;
    const growN = Math.max(1, Math.min(POINTS - 1, f.grow * (POINTS - 1)));
    const whole = Math.floor(growN);
    const part = growN - whole;
    for (let b = 0; b < shades.length; b++) {
      const gkey = `${b}`;
      let grad = this.gradients.get(gkey);
      if (!grad) {
        grad = ctx.createLinearGradient(0, top, 0, top - baseLen * 1.2);
        grad.addColorStop(0, shades[b][0]);
        grad.addColorStop(1, shades[b][1]);
        this.gradients.set(gkey, grad);
      }
      ctx.strokeStyle = grad;
      ctx.beginPath();
      for (let i = 0; i < n; i++) {
        if (Math.min(shades.length - 1, Math.floor(S.shade[i] * shades.length)) !== b) continue;
        const o = i * POINTS;
        ctx.moveTo(xs[o], ys[o]);
        for (let j = 1; j <= whole; j++) ctx.lineTo(xs[o + j], ys[o + j]);
        if (part > 0 && whole < POINTS - 1)
          ctx.lineTo(xs[o + whole] + (xs[o + whole + 1] - xs[o + whole]) * part, ys[o + whole] + (ys[o + whole + 1] - ys[o + whole]) * part);
      }
      ctx.stroke();
    }
    ctx.globalAlpha = 1;

    // The guides, as the viewport draws them: thicker, with their points.
    if (p.showGuides) {
      const gx = new Float32Array(POINTS);
      const gy = new Float32Array(POINTS);
      ctx.strokeStyle = '#3a5ea3';
      ctx.fillStyle = '#ffffff';
      ctx.lineWidth = Math.max(1.6, unit / 220);
      for (let g = 0; g < GUIDES; g++) {
        const u = g / (GUIDES - 1);
        this.grow(u, shape(u, 1, 0), cx, cy, R, span, f, sway, gx, gy, 0);
        ctx.beginPath();
        ctx.moveTo(gx[0], gy[0]);
        for (let j = 1; j < POINTS; j++) ctx.lineTo(gx[j], gy[j]);
        ctx.stroke();
        for (let j = 0; j < POINTS; j += 3) {
          ctx.beginPath();
          ctx.arc(gx[j], gy[j], 2.4, 0, Math.PI * 2);
          ctx.fill();
          ctx.stroke();
        }
      }
    }

    // The scalp in front of the roots.
    ctx.beginPath();
    ctx.arc(cx, cy, R, Math.PI * 1.5 - span - 0.2, Math.PI * 1.5 + span + 0.2);
    ctx.lineTo(W + 10, H + 10);
    ctx.lineTo(-10, H + 10);
    ctx.closePath();
    ctx.fillStyle = '#ececf0';
    ctx.fill();
    ctx.beginPath();
    ctx.arc(cx, cy, R, Math.PI * 1.5 - span - 0.2, Math.PI * 1.5 + span + 0.2);
    ctx.strokeStyle = '#cfcfd6';
    ctx.lineWidth = 1;
    ctx.stroke();
  }

  /** Grows one strand from root `u` with its shape into (xs, ys) at slot `slot`. */
  private grow(
    u: number,
    { L, bend }: { L: number; bend: number },
    cx: number,
    cy: number,
    R: number,
    span: number,
    f: Frame,
    sway: number,
    xs: Float32Array,
    ys: Float32Array,
    slot: number,
  ) {
    const phi = Math.PI * 1.5 - span + u * span * 2;
    let x = cx + Math.cos(phi) * (R - 1.5);
    let y = cy + Math.sin(phi) * (R - 1.5);
    const a0 = phi + (Math.PI * 1.5 - phi) * 0.15;
    const seg = L / (POINTS - 1);
    const o = slot * POINTS;
    const wave = sway * 0.14 * Math.sin(f.time * 0.9 + u * 4.2) + sway * 0.05 * Math.sin(f.time * 1.7 + u * 9);
    xs[o] = x;
    ys[o] = y;
    for (let j = 1; j < POINTS; j++) {
      const s = j / (POINTS - 1);
      const a = a0 + bend * Math.pow(s, 1.5) + wave * s * s;
      x += Math.cos(a) * seg;
      y += Math.sin(a) * seg;
      xs[o + j] = x;
      ys[o + j] = y;
    }
  }
}

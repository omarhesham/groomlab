// SPDX-License-Identifier: Apache-2.0
/*
 * The landing page's groom in 3D, the same little graph as strand-engine.ts with the same params:
 * Scatter puts roots on a head's scalp, Interpolate grows each strand from the three guides nearest
 * it, then Clump, Curl and Noise displace it, in that order, and HairColor colors it root to tip.
 * The head is a HeadShape (head-mesh.ts): the Manny sample head once it has loaded. It writes
 * StrandData, so the viewer never knows (or cares) that the source is procedural. The home page's
 * stand-in only: it draws at once, while the real GroomLab engine (home-engine.ts) loads, and where
 * the engine cannot run (no WebAssembly, no workers).
 */
import { ellipsoidHead, headRootMesh, type HeadShape } from './head-mesh';
import { hairShadeRGB, type StrandParams } from './strand-engine';
import type { RootMesh, StrandData } from './strand-data';

export const MAX_STRANDS = 24000;
const MAX_POINTS = 64;
const GUIDES = 48;
const MAX_CLUMPS = 480;
const GOLDEN = Math.PI * (3 - Math.sqrt(5));

/** A stable random number in [0, 1) for (a, b). */
function hash(a: number, b: number) {
  let h = Math.imul(a ^ 0x9e3779b9, 0x85ebca6b) ^ Math.imul(b + 0x632be59b, 0xc2b2ae35);
  h ^= h >>> 16;
  h = Math.imul(h, 0x7feb352d);
  h ^= h >>> 15;
  h = Math.imul(h, 0x846ca68b);
  h ^= h >>> 16;
  return (h >>> 0) / 4294967296;
}

/**
 * How far inside the hairline direction (x, y, z) from the cranium's center is: above 0 on the
 * scalp. The front sits higher than the sides, the nape lower; `drop` lowers it all (a big head of
 * curls grows from further down the forehead and the nape).
 */
const hairline = (x: number, y: number, z: number, drop = 0) =>
  y + drop - (z > 0 ? -0.07 + 0.5 * z : -0.02 + 0.62 * z) + 0.05 * Math.abs(x) * (z > 0 ? 1 : 0);

/** The share of all directions that are on the scalp, per drop. */
const capShares = new Map<number, number>();
function capShare(drop: number) {
  let share = capShares.get(drop);
  if (share === undefined) {
    const N = 20000;
    let k = 0;
    for (let i = 0; i < N; i++) {
      const y = 1 - (2 * (i + 0.5)) / N;
      const r = Math.sqrt(1 - y * y);
      if (hairline(r * Math.cos(i * GOLDEN), y, r * Math.sin(i * GOLDEN), drop) > 0) k++;
    }
    share = k / N;
    capShares.set(drop, share);
  }
  return share;
}

/**
 * About `target` evenly spread directions on the scalp (a Fibonacci lattice, turned by the seed and
 * jittered by up to `jitter` of its spacing) into `out` as xyz. Returns how many it wrote.
 */
function capLattice(target: number, seed: number, salt: number, jitter: number, drop: number, out: Float32Array, max: number) {
  const N = Math.max(8, Math.round(target / capShare(drop)));
  const turn = hash(seed, salt) * Math.PI * 2;
  const spacing = Math.sqrt((4 * Math.PI) / N);
  let k = 0;
  for (let i = 0; i < N && k < max; i++) {
    const y0 = 1 - (2 * (i + 0.5)) / N;
    const r = Math.sqrt(Math.max(0, 1 - y0 * y0));
    const a = i * GOLDEN + turn;
    let x = r * Math.cos(a);
    let y = y0;
    let z = r * Math.sin(a);
    if (jitter > 0) {
      const j = spacing * jitter;
      x += (hash(i * 3 + 1, seed + salt) - 0.5) * j;
      y += (hash(i * 3 + 2, seed + salt) - 0.5) * j;
      z += (hash(i * 3 + 3, seed + salt) - 0.5) * j;
      const l = Math.hypot(x, y, z);
      x /= l;
      y /= l;
      z /= l;
    }
    if (hairline(x, y, z, drop) <= 0) continue;
    out[k * 3] = x;
    out[k * 3 + 1] = y;
    out[k * 3 + 2] = z;
    k++;
  }
  return k;
}

const graphite = [
  [0.23, 0.23, 0.25],
  [0.33, 0.33, 0.36],
  [0.43, 0.43, 0.45],
];
const markRGB = [
  [1, 0.302, 0.31],
  [1, 0.773, 0.239],
  [0.184, 0.482, 0.965],
];

export interface GroomStats {
  strands: number;
  guides: number;
}

export class ProceduralGroom {
  private head: HeadShape = ellipsoidHead();
  private positions = new Float32Array(0);
  private colors = new Float32Array(0);
  private offsets = new Uint32Array(MAX_STRANDS + 1);
  /** Root directions from the cranium's center (unit). */
  private roots = new Float32Array(MAX_STRANDS * 3);
  private n = 0;
  private P = 0;
  private gRoots = new Float32Array(GUIDES * 3);
  private gCount = 0;
  private gPos = new Float32Array(GUIDES * MAX_POINTS * 3);
  private gOffsets = new Uint32Array(GUIDES + 1);
  /** The three guides nearest each strand, and their weights. */
  private near = new Uint8Array(MAX_STRANDS * 3);
  private weight = new Float32Array(MAX_STRANDS * 3);
  private cRoots = new Float32Array(MAX_CLUMPS * 3);
  private cCount = 0;
  private cPos = new Float32Array(MAX_CLUMPS * MAX_POINTS * 3);
  private clumpOf = new Uint16Array(MAX_STRANDS);
  private rootsKey = '';
  private guidesKey = '';
  private clumpsKey = '';
  private colorKey = '';
  /** How far the hairline is lowered (from the last build's volume). */
  private drop = 0;

  /** Grows on this head from the next build on. */
  setHead(head: HeadShape) {
    this.head = head;
    this.rootsKey = '';
    this.colorKey = '';
  }

  /** The point on the skin in direction (x, y, z), into out at o. */
  private skin(x: number, y: number, z: number, out: Float32Array, o: number, lift = 1) {
    const r = this.head.radius(x, y, z) * lift;
    out[o] = x * r;
    out[o + 1] = y * r;
    out[o + 2] = z * r;
  }

  /** Pushes the point at out[o..o+2] out of the head, to `shell` times its radius. */
  private pushOut(out: Float32Array, o: number, shell: number) {
    const x = out[o];
    const y = out[o + 1];
    const z = out[o + 2];
    const l = Math.sqrt(x * x + y * y + z * z);
    if (l === 0) return;
    const R = this.head.radius(x / l, y / l, z / l) * shell;
    if (l >= R) return;
    const k = R / l;
    out[o] *= k;
    out[o + 1] *= k;
    out[o + 2] *= k;
  }

  /**
   * The scalp: the head painted under the hair with its root color (as a groom's scalp texture
   * would be), so the hair reads full without hundreds of thousands of strands. `skin` is the
   * head's own color (sRGB 0 to 1).
   */
  scalp(p: StrandParams, skin: number[] = [0.7, 0.68, 0.66]): RootMesh {
    const root =
      p.color === 'mark'
        ? [0.55, 0.5, 0.52]
        : p.color
          ? hairShadeRGB(p.color)[2][0].map((v) => (v / 255) * 0.7)
          : graphite[0];
    return headRootMesh(this.head, (x, y, z, out) => {
      const l = Math.hypot(x, y, z) || 1;
      // How far inside the hairline this vertex is, eased over a short band.
      const e = Math.max(0, Math.min(1, (hairline(x / l, y / l, z / l, this.drop) + 0.06) / 0.14));
      const k = e * e * (3 - 2 * e);
      for (let i = 0; i < 3; i++) out[i] = skin[i] + (root[i] - skin[i]) * k;
    });
  }

  stats(): GroomStats {
    return { strands: this.n, guides: this.gCount };
  }

  /** Points per strand for these params: enough to draw the curl's turns smoothly. */
  private points(p: StrandParams) {
    const L = p.length ?? 1;
    const base = Math.round(16 + 6 * L);
    // About a dozen points a turn, so a curl reads as a spiral, not a zigzag.
    const turns = p.curlFrequency * Math.max(1, L);
    return p.curl > 0 ? Math.max(base, Math.min(MAX_POINTS, Math.round(10 + turns * 12))) : Math.min(MAX_POINTS, base);
  }

  /** The guides, as StrandData (for the viewer to draw when they are shown). */
  guides(): StrandData {
    return { count: this.gCount, offsets: this.gOffsets, positions: this.gPos };
  }

  /** Grows `count` strands with these params. */
  build(p: StrandParams, count: number): StrandData {
    const seed = p.seed | 0;
    const P = this.points(p);
    const n = Math.max(1, Math.min(MAX_STRANDS, Math.round(count)));
    if (this.positions.length < n * P * 3) {
      this.positions = new Float32Array(n * P * 3);
      this.colors = new Float32Array(n * P * 3);
      this.colorKey = '';
    }

    const volume = p.volume ?? 0;
    const rk = `${seed}:${n}:${volume}`;
    if (rk !== this.rootsKey) {
      this.rootsKey = rk;
      this.drop = 0.16 * volume;
      this.n = capLattice(n, seed, 11, 0.85, this.drop, this.roots, MAX_STRANDS);
      this.guidesKey = '';
      this.clumpsKey = '';
      this.colorKey = '';
    }
    const gk = `${seed}:${P}:${volume}`;
    if (gk !== this.guidesKey) {
      this.guidesKey = gk;
      this.P = P;
      this.makeGuides(seed, P, volume);
      this.clumpsKey = '';
      this.colorKey = '';
    }
    const K = Math.max(2, Math.min(MAX_CLUMPS, Math.round(p.clumpCount * 3)));
    const ck = `${gk}:${K}`;
    if (ck !== this.clumpsKey) {
      this.clumpsKey = ck;
      this.makeClumps(seed, K, P);
      this.colorKey = '';
    }
    this.grow(p, seed);
    return { count: this.n, offsets: this.offsets, positions: this.positions };
  }

  /** Per-point colors for the strands of the last build; null when they have not changed. */
  colorize(p: StrandParams): Float32Array | null {
    const key = `${this.rootsKey}:${this.P}:${this.clumpsKey}:${JSON.stringify(p.color)}`;
    if (key === this.colorKey) return null;
    this.colorKey = key;
    const { n, P, colors, clumpOf } = this;
    const shades =
      p.color && p.color !== 'mark'
        ? hairShadeRGB(p.color).map(([a, b]) => [a.map((v) => v / 255), b.map((v) => v / 255)])
        : null;
    for (let i = 0; i < n; i++) {
      const o = i * P * 3;
      if (p.color === 'mark') {
        const c = markRGB[clumpOf[i] % 3];
        for (let j = 0; j < P; j++) colors.set(c, o + j * 3);
        continue;
      }
      if (!shades) {
        const c = graphite[Math.floor(hash(i, 5) * 3)];
        for (let j = 0; j < P; j++) colors.set(c, o + j * 3);
        continue;
      }
      const [root, tip] = shades[Math.floor(hash(i, 5) * 4)];
      // A little lightness per strand, so neighbors never read as one flat color.
      const v = 0.9 + 0.2 * hash(i, 9);
      for (let j = 0; j < P; j++) {
        const s = j / (P - 1);
        const t = s * s * (3 - 2 * s);
        const q = o + j * 3;
        colors[q] = (root[0] + (tip[0] - root[0]) * t) * v;
        colors[q + 1] = (root[1] + (tip[1] - root[1]) * t) * v;
        colors[q + 2] = (root[2] + (tip[2] - root[2]) * t) * v;
      }
    }
    return colors;
  }

  /**
   * Grows a guide from scalp direction (dx, dy, dz): it leaves the scalp at an angle, then lies
   * down, combed back and down with gravity, never into the head. With `volume` it stands off the
   * head instead, out and up, for a big head of curls.
   */
  private growGuide(dx: number, dy: number, dz: number, L: number, droop: number, volume: number, P: number, out: Float32Array, o: number) {
    this.skin(dx, dy, dz, out, o);
    let x = out[o];
    let y = out[o + 1];
    let z = out[o + 2];
    const seg = L / (P - 1);
    for (let j = 1; j < P; j++) {
      const s = (j - 0.5) / (P - 1);
      const l = Math.hypot(x, y, z) || 1;
      const nx = x / l;
      const ny = y / l;
      const nz = z / l;
      // Combed back from the hairline and out over the sides, along the scalp.
      let fx = nx * 0.7;
      let fy = -0.35;
      let fz = -1;
      let d = fx * nx + fy * ny + fz * nz;
      fx -= nx * d;
      fy -= ny * d;
      fz -= nz * d;
      const fl = Math.hypot(fx, fy, fz) || 1;
      // Down, along the scalp.
      d = -ny;
      const gx = -nx * d;
      const gy = -1 - ny * d;
      const gz = -nz * d;
      const lift = (1.15 * (1 - s) * (1 - s) + 0.06) * (1 - volume) + (1.1 - 0.5 * s) * volume;
      const comb = 0.9 * (1 - 0.75 * volume);
      const g = (0.25 + 0.75 * s) * droop * (1 - 0.7 * volume);
      let vx = nx * lift + (fx / fl) * comb + gx * g;
      let vy = ny * lift + (fy / fl) * comb + gy * g + 0.25 * volume * (1 - s);
      let vz = nz * lift + (fz / fl) * comb + gz * g;
      const vl = Math.hypot(vx, vy, vz) || 1;
      vx /= vl;
      vy /= vl;
      vz /= vl;
      const p = o + j * 3;
      out[p] = x + vx * seg;
      out[p + 1] = y + vy * seg;
      out[p + 2] = z + vz * seg;
      this.pushOut(out, p, 1.035);
      x = out[p];
      y = out[p + 1];
      z = out[p + 2];
    }
  }

  private makeGuides(seed: number, P: number, volume: number) {
    this.gCount = capLattice(GUIDES, seed, 23, 0.3, this.drop, this.gRoots, GUIDES);
    const R = this.gRoots;
    for (let g = 0; g < this.gCount; g++) {
      const L = 0.62 * (0.88 + 0.24 * hash(g, seed + 31));
      const droop = 0.8 + 0.4 * hash(g, seed + 37);
      this.growGuide(R[g * 3], R[g * 3 + 1], R[g * 3 + 2], L, droop, volume, P, this.gPos, g * P * 3);
      this.gOffsets[g] = g * P;
    }
    this.gOffsets[this.gCount] = this.gCount * P;
    // Each strand's three nearest guides, by angle between roots.
    const { roots, near, weight } = this;
    for (let i = 0; i < this.n; i++) {
      const x = roots[i * 3];
      const y = roots[i * 3 + 1];
      const z = roots[i * 3 + 2];
      let b0 = -1, b1 = -1, b2 = -1;
      let d0 = 9, d1 = 9, d2 = 9;
      for (let g = 0; g < this.gCount; g++) {
        const d = 1 - (x * R[g * 3] + y * R[g * 3 + 1] + z * R[g * 3 + 2]);
        if (d < d0) {
          b2 = b1; d2 = d1; b1 = b0; d1 = d0; b0 = g; d0 = d;
        } else if (d < d1) {
          b2 = b1; d2 = d1; b1 = g; d1 = d;
        } else if (d < d2) {
          b2 = g; d2 = d;
        }
      }
      const w0 = 1 / (d0 + 1e-4);
      const w1 = 1 / (d1 + 1e-4);
      const w2 = 1 / (d2 + 1e-4);
      const sum = w0 + w1 + w2;
      near[i * 3] = b0;
      near[i * 3 + 1] = Math.max(0, b1);
      near[i * 3 + 2] = Math.max(0, b2);
      weight[i * 3] = w0 / sum;
      weight[i * 3 + 1] = b1 < 0 ? 0 : w1 / sum;
      weight[i * 3 + 2] = b2 < 0 ? 0 : w2 / sum;
    }
  }

  /**
   * Interpolates a strand at scalp direction (x, y, z) from guides a, b, c with weights into out at
   * o: the root on the skin plus the guides' blended shapes, scaled by `len`.
   */
  private interpolate(x: number, y: number, z: number, a: number, b: number, c: number, wa: number, wb: number, wc: number, len: number, out: Float32Array, o: number) {
    const { P, gPos } = this;
    this.skin(x, y, z, out, o);
    const rx = out[o];
    const ry = out[o + 1];
    const rz = out[o + 2];
    const ga = a * P * 3;
    const gb = b * P * 3;
    const gc = c * P * 3;
    const ax = gPos[ga], ay = gPos[ga + 1], az = gPos[ga + 2];
    const bx = gPos[gb], by = gPos[gb + 1], bz = gPos[gb + 2];
    const cx = gPos[gc], cy = gPos[gc + 1], cz = gPos[gc + 2];
    for (let q = 0; q < P * 3; q += 3) {
      out[o + q] = rx + (wa * (gPos[ga + q] - ax) + wb * (gPos[gb + q] - bx) + wc * (gPos[gc + q] - cx)) * len;
      out[o + q + 1] = ry + (wa * (gPos[ga + q + 1] - ay) + wb * (gPos[gb + q + 1] - by) + wc * (gPos[gc + q + 1] - cy)) * len;
      out[o + q + 2] = rz + (wa * (gPos[ga + q + 2] - az) + wb * (gPos[gb + q + 2] - bz) + wc * (gPos[gc + q + 2] - cz)) * len;
    }
  }

  private makeClumps(seed: number, K: number, P: number) {
    this.cCount = capLattice(K, seed, 41, 0.6, this.drop, this.cRoots, MAX_CLUMPS);
    const C = this.cRoots;
    const R = this.gRoots;
    for (let k = 0; k < this.cCount; k++) {
      const x = C[k * 3];
      const y = C[k * 3 + 1];
      const z = C[k * 3 + 2];
      // A clump center is a strand of its own, grown from its nearest guide.
      let best = 0;
      let bd = 9;
      for (let g = 0; g < this.gCount; g++) {
        const d = 1 - (x * R[g * 3] + y * R[g * 3 + 1] + z * R[g * 3 + 2]);
        if (d < bd) {
          bd = d;
          best = g;
        }
      }
      this.interpolate(x, y, z, best, best, best, 1, 0, 0, 1.03, this.cPos, k * P * 3);
    }
    const { roots, clumpOf } = this;
    for (let i = 0; i < this.n; i++) {
      let best = 0;
      let bd = 9;
      for (let k = 0; k < this.cCount; k++) {
        const d = 1 - (roots[i * 3] * C[k * 3] + roots[i * 3 + 1] * C[k * 3 + 1] + roots[i * 3 + 2] * C[k * 3 + 2]);
        if (d < bd) {
          bd = d;
          best = k;
        }
      }
      clumpOf[i] = best;
    }
  }

  private grow(p: StrandParams, seed: number) {
    const { n, P, roots, near, weight, positions: out, cPos, clumpOf, offsets } = this;
    const curlR = p.curl * 0.12;
    const noiseA = p.noise * 0.055;
    const length = p.length ?? 1;
    const volume = p.volume ?? 0;
    // Per point along the strand: the clump pull, the curl's ramp and turn, the noise's waves.
    const T = this.tables(P, p.curlFrequency * Math.max(1, length));
    for (let i = 0; i < n; i++) {
      const o = i * P * 3;
      offsets[i] = i * P;
      const x = roots[i * 3];
      const y = roots[i * 3 + 1];
      const z = roots[i * 3 + 2];
      const len = (0.86 + 0.28 * hash(i, seed + 3)) * length;
      this.interpolate(x, y, z, near[i * 3], near[i * 3 + 1], near[i * 3 + 2], weight[i * 3], weight[i * 3 + 1], weight[i * 3 + 2], len, out, o);
      const k = clumpOf[i];
      const c = k * P * 3;
      const phase = hash(i, seed + 7) * Math.PI * 2;
      const ph = p.clump > 0 ? hash(k, seed + 13) * Math.PI * 2 + phase * 0.12 : phase;
      const cph = Math.cos(ph);
      const sph = Math.sin(ph);
      // Each strand's curl is a little wider or tighter than its neighbors'.
      const curlK = 0.8 + 0.4 * hash(i, seed + 47);
      const n1 = hash(i, seed + 17) * 6.283;
      const n2 = hash(i, seed + 19) * 6.283;
      const n3 = hash(i, seed + 23) * 6.283;
      const n4 = hash(i, seed + 29) * 6.283;
      const c1 = Math.cos(n1), s1 = Math.sin(n1);
      const c2 = Math.cos(n2), s2 = Math.sin(n2);
      const c3 = Math.cos(n3), s3 = Math.sin(n3);
      const c4 = Math.cos(n4), s4 = Math.sin(n4);
      // Each strand keeps its own height over the scalp, so the hair has some volume.
      const shell = 1.012 + (0.07 + 0.25 * volume) * hash(i, seed + 43);
      let px = out[o];
      let py = out[o + 1];
      let pz = out[o + 2];
      for (let j = 1; j < P; j++) {
        const q = o + j * 3;
        let bx = out[q];
        let by = out[q + 1];
        let bz = out[q + 2];
        // Clump: pull toward the clump's center strand, more toward the tip.
        if (p.clump > 0) {
          const t = p.clump * T.pull[j];
          // The clump's center strand, at this groom's length.
          bx += (cPos[c] + (cPos[c + j * 3] - cPos[c]) * length - bx) * t;
          by += (cPos[c + 1] + (cPos[c + j * 3 + 1] - cPos[c + 1]) * length - by) * t;
          bz += (cPos[c + 2] + (cPos[c + j * 3 + 2] - cPos[c + 2]) * length - bz) * t;
        }
        // The strand's frame here: along it, off the scalp, and across.
        let tx = bx - px;
        let ty = by - py;
        let tz = bz - pz;
        const tl = Math.sqrt(tx * tx + ty * ty + tz * tz) || 1;
        tx /= tl;
        ty /= tl;
        tz /= tl;
        px = bx;
        py = by;
        pz = bz;
        const bl = Math.sqrt(bx * bx + by * by + bz * bz) || 1;
        let nx = bx / bl;
        let ny = by / bl;
        let nz = bz / bl;
        const d = nx * tx + ny * ty + nz * tz;
        nx -= tx * d;
        ny -= ty * d;
        nz -= tz * d;
        const nl = Math.sqrt(nx * nx + ny * ny + nz * nz) || 1;
        nx /= nl;
        ny /= nl;
        nz /= nl;
        const vx = ty * nz - tz * ny;
        const vy = tz * nx - tx * nz;
        const vz = tx * ny - ty * nx;
        let a = 0;
        let b = 0;
        // Curl: a helix around the strand.
        if (curlR > 0) {
          const r = curlR * curlK * T.ramp[j];
          a += r * (T.cf[j] * cph - T.sf[j] * sph);
          b += r * (T.sf[j] * cph + T.cf[j] * sph);
        }
        // Noise: frizz, growing toward the tip.
        if (noiseA > 0) {
          const amp = noiseA * T.tip[j];
          a += amp * (T.s53[j] * c1 + T.c53[j] * s1 + 0.55 * (T.s131[j] * c2 + T.c131[j] * s2));
          b += amp * (T.s41[j] * c3 + T.c41[j] * s3 + 0.55 * (T.s117[j] * c4 + T.c117[j] * s4));
        }
        out[q] = bx + nx * a + vx * b;
        out[q + 1] = by + ny * a + vy * b;
        out[q + 2] = bz + nz * a + vz * b;
        this.pushOut(out, q, shell * (0.985 + 0.015 * T.s[j]));
      }
    }
    offsets[n] = n * P;
  }

  private tableKey = '';
  private table: Record<'s' | 'pull' | 'ramp' | 'tip' | 'cf' | 'sf' | 's53' | 'c53' | 's131' | 'c131' | 's41' | 'c41' | 's117' | 'c117', Float32Array> | null = null;

  private tables(P: number, freq: number) {
    const key = `${P}:${freq}`;
    if (this.table && key === this.tableKey) return this.table;
    this.tableKey = key;
    const f = (g: (s: number) => number) => Float32Array.from({ length: P }, (_, j) => g(j / (P - 1)));
    const w = Math.PI * 2 * freq;
    this.table = {
      s: f((s) => s),
      pull: f((s) => Math.pow(s, 0.7) * 0.9),
      ramp: f((s) => Math.min(1, s * 2.5)),
      tip: f((s) => Math.pow(s, 1.3)),
      cf: f((s) => Math.cos(w * s)),
      sf: f((s) => Math.sin(w * s)),
      s53: f((s) => Math.sin(s * 5.3)),
      c53: f((s) => Math.cos(s * 5.3)),
      s131: f((s) => Math.sin(s * 13.1)),
      c131: f((s) => Math.cos(s * 13.1)),
      s41: f((s) => Math.sin(s * 4.1)),
      c41: f((s) => Math.cos(s * 4.1)),
      s117: f((s) => Math.sin(s * 11.7)),
      c117: f((s) => Math.cos(s * 11.7)),
    };
    return this.table;
  }
}

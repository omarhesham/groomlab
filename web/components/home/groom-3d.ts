// SPDX-License-Identifier: Apache-2.0
/*
 * The landing page's groom in 3D, the same little graph as strand-engine.ts with the same params:
 * Scatter puts roots on a head's scalp, Interpolate grows each strand from the three guides nearest
 * it, then Clump, Curl and Noise displace it, in that order, and HairColor colors it root to tip.
 * It writes StrandData, so the viewer never knows (or cares) that the source is procedural.
 */
import { hairShadeRGB, type StrandParams } from './strand-engine';
import type { RootMesh, StrandData } from './strand-data';

/** The head: an ellipsoid with these radii (x across, y up, z forward). */
const HX = 0.84;
const HY = 1;
const HZ = 0.98;
export const MAX_STRANDS = 5000;
const MAX_POINTS = 48;
const GUIDES = 40;
const MAX_CLUMPS = 120;
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

/** The hairline: the front sits higher than the sides, the nape lower. */
const inCap = (x: number, y: number, z: number) => y > 0.06 + (z > 0 ? 0.46 : 0.5) * z;

const CAP_SHARE = (() => {
  const N = 20000;
  let k = 0;
  for (let i = 0; i < N; i++) {
    const y = 1 - (2 * (i + 0.5)) / N;
    const r = Math.sqrt(1 - y * y);
    if (inCap(r * Math.cos(i * GOLDEN), y, r * Math.sin(i * GOLDEN))) k++;
  }
  return k / N;
})();

/**
 * About `target` evenly spread directions on the scalp (a Fibonacci lattice, turned by the seed and
 * jittered by up to `jitter` of its spacing) into `out` as xyz. Returns how many it wrote.
 */
function capLattice(target: number, seed: number, salt: number, jitter: number, out: Float32Array, max: number) {
  const N = Math.max(8, Math.round(target / CAP_SHARE));
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
    if (!inCap(x, y, z)) continue;
    out[k * 3] = x;
    out[k * 3 + 1] = y;
    out[k * 3 + 2] = z;
    k++;
  }
  return k;
}

/** Pushes the point at out[o..o+2] out of the head, to `shell` times its radius. */
function pushOut(out: Float32Array, o: number, shell: number) {
  const qx = out[o] / HX;
  const qy = out[o + 1] / HY;
  const qz = out[o + 2] / HZ;
  const q = Math.sqrt(qx * qx + qy * qy + qz * qz);
  if (q >= shell || q === 0) return;
  const k = shell / q;
  out[o] *= k;
  out[o + 1] *= k;
  out[o + 2] *= k;
}

/** The scalp's outward normal under point (x, y, z), into n. */
function normalAt(x: number, y: number, z: number, n: number[]) {
  const nx = x / (HX * HX);
  const ny = y / (HY * HY);
  const nz = z / (HZ * HZ);
  const l = Math.sqrt(nx * nx + ny * ny + nz * nz) || 1;
  n[0] = nx / l;
  n[1] = ny / l;
  n[2] = nz / l;
}

/**
 * Grows a guide from scalp direction (dx, dy, dz): it leaves the scalp at an angle, then lies down,
 * combed back and down with gravity, never into the head.
 */
function growGuide(dx: number, dy: number, dz: number, L: number, droop: number, P: number, out: Float32Array, o: number) {
  let x = dx * HX;
  let y = dy * HY;
  let z = dz * HZ;
  out[o] = x;
  out[o + 1] = y;
  out[o + 2] = z;
  const seg = L / (P - 1);
  const n = [0, 0, 0];
  for (let j = 1; j < P; j++) {
    const s = (j - 0.5) / (P - 1);
    normalAt(x, y, z, n);
    // Combed back from the hairline and out over the sides, along the scalp.
    let fx = (x / HX) * 0.7;
    let fy = -0.35;
    let fz = -1;
    let d = fx * n[0] + fy * n[1] + fz * n[2];
    fx -= n[0] * d;
    fy -= n[1] * d;
    fz -= n[2] * d;
    const fl = Math.hypot(fx, fy, fz) || 1;
    // Down, along the scalp.
    d = -n[1];
    const gx = -n[0] * d;
    const gy = -1 - n[1] * d;
    const gz = -n[2] * d;
    const lift = 1.15 * (1 - s) * (1 - s) + 0.06;
    const g = (0.25 + 0.75 * s) * droop;
    let vx = n[0] * lift + (fx / fl) * 0.9 + gx * g;
    let vy = n[1] * lift + (fy / fl) * 0.9 + gy * g;
    let vz = n[2] * lift + (fz / fl) * 0.9 + gz * g;
    const vl = Math.hypot(vx, vy, vz) || 1;
    vx /= vl;
    vy /= vl;
    vz /= vl;
    const p = o + j * 3;
    out[p] = x + vx * seg;
    out[p + 1] = y + vy * seg;
    out[p + 2] = z + vz * seg;
    pushOut(out, p, 1.035);
    x = out[p];
    y = out[p + 1];
    z = out[p + 2];
  }
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
  private positions = new Float32Array(MAX_STRANDS * MAX_POINTS * 3);
  private colors = new Float32Array(MAX_STRANDS * MAX_POINTS * 3);
  private offsets = new Uint32Array(MAX_STRANDS + 1);
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
  private clumpOf = new Uint8Array(MAX_STRANDS);
  private rootsKey = '';
  private guidesKey = '';
  private clumpsKey = '';
  private colorKey = '';

  /**
   * The scalp: the head's ellipsoid, painted under the hair with its root color (as a groom's scalp
   * texture would be), so the hair reads full without tens of thousands of strands.
   */
  scalp(p: StrandParams): RootMesh {
    const W = 64;
    const H = 40;
    const positions = new Float32Array((W + 1) * (H + 1) * 3);
    const normals = new Float32Array(positions.length);
    const colors = new Float32Array(positions.length);
    const skin = [0.86, 0.84, 0.82];
    const root =
      p.color === 'mark'
        ? [0.55, 0.5, 0.52]
        : p.color
          ? hairShadeRGB(p.color)[2][0].map((v) => (v / 255) * 0.8)
          : graphite[0];
    const n = [0, 0, 0];
    for (let r = 0, v = 0; r <= H; r++) {
      const th = (r / H) * Math.PI;
      for (let c = 0; c <= W; c++, v++) {
        const ph = (c / W) * Math.PI * 2;
        const x = Math.sin(th) * Math.sin(ph) * HX;
        const y = Math.cos(th) * HY;
        const z = Math.sin(th) * Math.cos(ph) * HZ;
        positions.set([x, y, z], v * 3);
        normalAt(x, y, z, n);
        normals.set(n, v * 3);
        // How far inside the hairline this vertex is, eased over a short band.
        const dy = y / HY;
        const dz = z / HZ;
        const e = Math.max(0, Math.min(1, (dy - (0.06 + (dz > 0 ? 0.46 : 0.5) * dz) + 0.07) / 0.16));
        const k = e * e * (3 - 2 * e);
        for (let i = 0; i < 3; i++) colors[v * 3 + i] = skin[i] + (root[i] - skin[i]) * k;
      }
    }
    const indices = new Uint32Array(W * H * 6);
    for (let r = 0, i = 0; r < H; r++)
      for (let c = 0; c < W; c++) {
        const a = r * (W + 1) + c;
        const b = a + W + 1;
        indices.set([a, b, a + 1, b, b + 1, a + 1], i);
        i += 6;
      }
    return { positions, normals, indices, colors };
  }

  stats(): GroomStats {
    return { strands: this.n, guides: this.gCount };
  }

  /** Points per strand for these params: enough to draw the curl's turns smoothly. */
  private points(p: StrandParams) {
    return p.curl > 0 ? Math.max(20, Math.min(MAX_POINTS, Math.round(14 + p.curlFrequency * 4.5))) : 20;
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

    const rk = `${seed}:${n}`;
    if (rk !== this.rootsKey) {
      this.rootsKey = rk;
      this.n = capLattice(n, seed, 11, 0.85, this.roots, MAX_STRANDS);
      this.guidesKey = '';
      this.clumpsKey = '';
      this.colorKey = '';
    }
    const gk = `${seed}:${P}`;
    if (gk !== this.guidesKey) {
      this.guidesKey = gk;
      this.P = P;
      this.makeGuides(seed, P);
      this.clumpsKey = '';
      this.colorKey = '';
    }
    const K = Math.max(2, Math.min(MAX_CLUMPS, Math.round(p.clumpCount * 3)));
    const ck = `${seed}:${K}:${P}`;
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
      for (let j = 0; j < P; j++) {
        const s = j / (P - 1);
        const t = s * s * (3 - 2 * s);
        const q = o + j * 3;
        colors[q] = root[0] + (tip[0] - root[0]) * t;
        colors[q + 1] = root[1] + (tip[1] - root[1]) * t;
        colors[q + 2] = root[2] + (tip[2] - root[2]) * t;
      }
    }
    return colors;
  }

  private makeGuides(seed: number, P: number) {
    this.gCount = capLattice(GUIDES, seed, 23, 0.3, this.gRoots, GUIDES);
    const R = this.gRoots;
    for (let g = 0; g < this.gCount; g++) {
      const L = 0.56 * (0.88 + 0.24 * hash(g, seed + 31));
      const droop = 0.8 + 0.4 * hash(g, seed + 37);
      growGuide(R[g * 3], R[g * 3 + 1], R[g * 3 + 2], L, droop, P, this.gPos, g * P * 3);
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
   * o: the root on the scalp plus the guides' blended shapes, scaled by `len`.
   */
  private interpolate(x: number, y: number, z: number, a: number, b: number, c: number, wa: number, wb: number, wc: number, len: number, out: Float32Array, o: number) {
    const { P, gPos } = this;
    const rx = x * HX;
    const ry = y * HY;
    const rz = z * HZ;
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
    this.cCount = capLattice(K, seed, 41, 0.6, this.cRoots, MAX_CLUMPS);
    const C = this.cRoots;
    const R = this.gRoots;
    for (let k = 0; k < this.cCount; k++) {
      const x = C[k * 3];
      const y = C[k * 3 + 1];
      const z = C[k * 3 + 2];
      // A clump center is a strand of its own, grown from the nearest guide's neighbors.
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
    const curlR = p.curl * 0.07;
    const noiseA = p.noise * 0.055;
    // Per point along the strand: the clump pull, the curl's ramp and turn, the noise's waves.
    const T = this.tables(P, p.curlFrequency);
    for (let i = 0; i < n; i++) {
      const o = i * P * 3;
      offsets[i] = i * P;
      const x = roots[i * 3];
      const y = roots[i * 3 + 1];
      const z = roots[i * 3 + 2];
      const len = 0.86 + 0.28 * hash(i, seed + 3);
      this.interpolate(x, y, z, near[i * 3], near[i * 3 + 1], near[i * 3 + 2], weight[i * 3], weight[i * 3 + 1], weight[i * 3 + 2], len, out, o);
      const k = clumpOf[i];
      const c = k * P * 3;
      const phase = hash(i, seed + 7) * Math.PI * 2;
      const ph = p.clump > 0 ? hash(k, seed + 13) * Math.PI * 2 + phase * 0.12 : phase;
      const cph = Math.cos(ph);
      const sph = Math.sin(ph);
      const n1 = hash(i, seed + 17) * 6.283;
      const n2 = hash(i, seed + 19) * 6.283;
      const n3 = hash(i, seed + 23) * 6.283;
      const n4 = hash(i, seed + 29) * 6.283;
      const c1 = Math.cos(n1), s1 = Math.sin(n1);
      const c2 = Math.cos(n2), s2 = Math.sin(n2);
      const c3 = Math.cos(n3), s3 = Math.sin(n3);
      const c4 = Math.cos(n4), s4 = Math.sin(n4);
      // Each strand keeps its own height over the scalp, so the hair has some volume.
      const shell = 1.012 + 0.07 * hash(i, seed + 43);
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
          bx += (cPos[c + j * 3] - bx) * t;
          by += (cPos[c + j * 3 + 1] - by) * t;
          bz += (cPos[c + j * 3 + 2] - bz) * t;
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
        let nx = bx / (HX * HX);
        let ny = by / (HY * HY);
        let nz = bz / (HZ * HZ);
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
          const r = curlR * T.ramp[j];
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
        pushOut(out, q, shell * (0.985 + 0.015 * T.s[j]));
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

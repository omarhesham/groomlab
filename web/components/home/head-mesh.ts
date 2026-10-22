// SPDX-License-Identifier: Apache-2.0
/*
 * The head the landing page's hair grows on. Its shape is a radial map: the distance from the
 * cranium's center to the skin in every direction, kept in a small cube map. The procedural groom
 * places roots, combs guides and keeps strands out of the head with it, whatever the head is: an
 * ellipsoid until the real one loads, then the Manny sample head ("Manny head by Nick Rutlinh
 * (GroomLab), CC BY 4.0"), whose subdivision cage (web/public/samples/manny_head.bin, written by
 * tools/web/make_head_mesh.py) is smoothed here with Catmull-Clark.
 */
import type { RootMesh } from './strand-data';

/** Cells per cube face edge. */
const RES = 40;

export interface HeadShape {
  /** Distance from the center (the origin) to the skin along unit direction (x, y, z). */
  radius(x: number, y: number, z: number): number;
  /** The head's surface as a mesh (y up, facing +z, the cranium's center at the origin). */
  mesh: { positions: Float32Array; indices: Uint32Array };
}

/** The cube face and cell for a direction, and the in-face coordinates (0 to RES - 1). */
function faceUV(x: number, y: number, z: number, out: number[]) {
  const ax = Math.abs(x);
  const ay = Math.abs(y);
  const az = Math.abs(z);
  let f: number;
  let u: number;
  let v: number;
  if (ax >= ay && ax >= az) {
    f = x > 0 ? 0 : 1;
    u = z / ax;
    v = y / ax;
  } else if (ay >= az) {
    f = y > 0 ? 2 : 3;
    u = x / ay;
    v = z / ay;
  } else {
    f = z > 0 ? 4 : 5;
    u = x / az;
    v = y / az;
  }
  out[0] = f;
  out[1] = ((u + 1) / 2) * (RES - 1);
  out[2] = ((v + 1) / 2) * (RES - 1);
}

/** The direction through cell (i, j) of face f (i, j may step one past the face's edge). */
function cellDir(f: number, i: number, j: number, out: number[]) {
  const u = (i / (RES - 1)) * 2 - 1;
  const v = (j / (RES - 1)) * 2 - 1;
  const s = f % 2 === 0 ? 1 : -1;
  let x: number, y: number, z: number;
  if (f < 2) [x, y, z] = [s, v, u];
  else if (f < 4) [x, y, z] = [u, s, v];
  else [x, y, z] = [u, v, s];
  const l = Math.hypot(x, y, z);
  out[0] = x / l;
  out[1] = y / l;
  out[2] = z / l;
}

/** A radial cube map from points on a closed-ish surface around the origin: the farthest point per cell. */
function radialMap(points: Float32Array): Float32Array {
  const map = new Float32Array(6 * RES * RES);
  const fuv = [0, 0, 0];
  for (let p = 0; p < points.length; p += 3) {
    const x = points[p];
    const y = points[p + 1];
    const z = points[p + 2];
    const r = Math.hypot(x, y, z);
    if (r < 1e-6) continue;
    faceUV(x, y, z, fuv);
    const c = fuv[0] * RES * RES + Math.round(fuv[2]) * RES + Math.round(fuv[1]);
    if (r > map[c]) map[c] = r;
  }
  // Fill cells no point landed in (the open neck) from their neighbors, across face edges too.
  const d = [0, 0, 0];
  for (let pass = 0; pass < RES * 2; pass++) {
    let empty = 0;
    const next = map.slice();
    for (let f = 0; f < 6; f++)
      for (let j = 0; j < RES; j++)
        for (let i = 0; i < RES; i++) {
          const c = f * RES * RES + j * RES + i;
          if (map[c] > 0) continue;
          let sum = 0;
          let n = 0;
          for (const [di, dj] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
            cellDir(f, i + di, j + dj, d);
            faceUV(d[0], d[1], d[2], fuv);
            const v = map[fuv[0] * RES * RES + Math.round(fuv[2]) * RES + Math.round(fuv[1])];
            if (v > 0) {
              sum += v;
              n++;
            }
          }
          if (n) next[c] = sum / n;
          else empty++;
        }
    map.set(next);
    if (!empty) break;
  }
  return map;
}

function lookup(map: Float32Array, x: number, y: number, z: number, fuv: number[]) {
  faceUV(x, y, z, fuv);
  const f = fuv[0] * RES * RES;
  const i0 = Math.min(RES - 2, Math.floor(fuv[1]));
  const j0 = Math.min(RES - 2, Math.floor(fuv[2]));
  const a = fuv[1] - i0;
  const b = fuv[2] - j0;
  const r0 = map[f + j0 * RES + i0] * (1 - a) + map[f + j0 * RES + i0 + 1] * a;
  const r1 = map[f + (j0 + 1) * RES + i0] * (1 - a) + map[f + (j0 + 1) * RES + i0 + 1] * a;
  return r0 * (1 - b) + r1 * b;
}

/** An ellipsoid head with these radii (the stand-in until the real head has loaded). */
export function ellipsoidHead(hx = 0.84, hy = 1, hz = 0.94): HeadShape {
  const W = 64;
  const H = 40;
  const positions = new Float32Array((W + 1) * (H + 1) * 3);
  for (let r = 0, v = 0; r <= H; r++) {
    const th = (r / H) * Math.PI;
    for (let c = 0; c <= W; c++, v++) {
      const ph = (c / W) * Math.PI * 2;
      positions[v * 3] = Math.sin(th) * Math.sin(ph) * hx;
      positions[v * 3 + 1] = Math.cos(th) * hy;
      positions[v * 3 + 2] = Math.sin(th) * Math.cos(ph) * hz;
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
  return {
    radius: (x, y, z) => 1 / Math.sqrt((x / hx) ** 2 + (y / hy) ** 2 + (z / hz) ** 2),
    mesh: { positions, indices },
  };
}

/** One Catmull-Clark step over triangles and quads (open edges kept as creases); returns quads. */
function subdivide(pos: Float32Array<ArrayBuffer>, faces: number[][]): { pos: Float32Array<ArrayBuffer>; faces: number[][] } {
  const nv = pos.length / 3;
  const edgeOf = new Map<number, number>();
  const edgeA: number[] = [];
  const edgeB: number[] = [];
  const edgeFaces: number[][] = [];
  const faceEdges: number[][] = [];
  const key = (a: number, b: number) => (a < b ? a * 65536 + b : b * 65536 + a);
  faces.forEach((f, fi) => {
    faceEdges.push(
      f.map((a, k) => {
        const b = f[(k + 1) % f.length];
        let e = edgeOf.get(key(a, b));
        if (e === undefined) {
          e = edgeA.length;
          edgeOf.set(key(a, b), e);
          edgeA.push(a);
          edgeB.push(b);
          edgeFaces.push([]);
        }
        edgeFaces[e].push(fi);
        return e;
      }),
    );
  });
  const ne = edgeA.length;
  const nf = faces.length;
  const out = new Float32Array((nv + ne + nf) * 3);
  // Face points.
  const fp = nv + ne;
  faces.forEach((f, fi) => {
    for (const a of f) for (let k = 0; k < 3; k++) out[(fp + fi) * 3 + k] += pos[a * 3 + k] / f.length;
  });
  // Edge points.
  for (let e = 0; e < ne; e++) {
    const a = edgeA[e];
    const b = edgeB[e];
    const fs = edgeFaces[e];
    for (let k = 0; k < 3; k++) {
      const mid = (pos[a * 3 + k] + pos[b * 3 + k]) / 2;
      out[(nv + e) * 3 + k] =
        fs.length === 2 ? (mid * 2 + out[(fp + fs[0]) * 3 + k] + out[(fp + fs[1]) * 3 + k]) / 4 : mid;
    }
  }
  // Vertex points.
  const vFaces: number[][] = Array.from({ length: nv }, () => []);
  const vEdges: number[][] = Array.from({ length: nv }, () => []);
  faces.forEach((f, fi) => f.forEach((a) => vFaces[a].push(fi)));
  for (let e = 0; e < ne; e++) {
    vEdges[edgeA[e]].push(e);
    vEdges[edgeB[e]].push(e);
  }
  for (let v = 0; v < nv; v++) {
    const open = vEdges[v].filter((e) => edgeFaces[e].length < 2);
    for (let k = 0; k < 3; k++) {
      const P = pos[v * 3 + k];
      if (open.length >= 2) {
        let s = 0;
        for (const e of open.slice(0, 2)) s += pos[(edgeA[e] === v ? edgeB[e] : edgeA[e]) * 3 + k];
        out[v * 3 + k] = P * 0.75 + s / 8;
        continue;
      }
      const n = vEdges[v].length;
      let F = 0;
      for (const fi of vFaces[v]) F += out[(fp + fi) * 3 + k];
      F /= vFaces[v].length;
      let R = 0;
      for (const e of vEdges[v]) R += (pos[edgeA[e] * 3 + k] + pos[edgeB[e] * 3 + k]) / 2;
      R /= n;
      out[v * 3 + k] = (F + 2 * R + (n - 3) * P) / n;
    }
  }
  const quads: number[][] = [];
  faces.forEach((f, fi) => {
    const es = faceEdges[fi];
    for (let k = 0; k < f.length; k++) {
      const prev = es[(k + f.length - 1) % f.length];
      quads.push([f[k], nv + es[k], fp + fi, nv + prev]);
    }
  });
  return { pos: out, faces: quads };
}

/** Reads the head cage, smooths it, and moves it into groom space (the cranium's center at the origin). */
export function parseHead(buffer: ArrayBuffer): HeadShape {
  const view = new DataView(buffer);
  const magic = String.fromCharCode(...new Uint8Array(buffer, 0, 4));
  if (magic !== 'GLHM' || view.getUint16(4, true) !== 1) throw new Error('not a head mesh');
  const nv = view.getUint16(6, true);
  const nf = view.getUint16(8, true);
  const scale = view.getFloat32(12, true);
  // Centimetres to groom units: the cranium about 1 across from its center, which sits ~3 cm up.
  const k = scale * 0.1;
  const center = [0, 3.1, -0.4];
  let pos = new Float32Array(nv * 3);
  let o = 16;
  for (let i = 0; i < nv * 3; i++, o += 2) pos[i] = view.getInt16(o, true) * k - center[i % 3] * 0.1;
  let faces: number[][] = [];
  for (let f = 0; f < nf; f++) {
    const q = [0, 1, 2, 3].map((i) => view.getUint16(o + i * 2, true));
    o += 8;
    faces.push(q[3] === 65535 ? q.slice(0, 3) : q);
  }
  for (let level = 0; level < 2; level++) ({ pos, faces } = subdivide(pos, faces));
  const indices = new Uint32Array(faces.length * 6);
  faces.forEach((q, i) => indices.set([q[0], q[1], q[2], q[0], q[2], q[3]], i * 6));
  // Points for the radial map: every vertex and the middle of every quad's edges and center.
  const samples = new Float32Array(pos.length + faces.length * 3 * 3);
  samples.set(pos);
  let s = pos.length;
  for (const q of faces) {
    for (let k2 = 0; k2 < 3; k2++) {
      const a = pos[q[0] * 3 + k2];
      const b = pos[q[1] * 3 + k2];
      const c = pos[q[2] * 3 + k2];
      const d = pos[q[3] * 3 + k2];
      samples[s + k2] = (a + b + c + d) / 4;
      samples[s + 3 + k2] = (a + b) / 2;
      samples[s + 6 + k2] = (a + d) / 2;
    }
    s += 9;
  }
  const map = radialMap(samples);
  const fuv = [0, 0, 0];
  return { radius: (x, y, z) => lookup(map, x, y, z, fuv), mesh: { positions: pos, indices } };
}

let pending: Promise<HeadShape | null> | null = null;

/** The Manny head, fetched once per page; null when it can't be had (the ellipsoid stays). */
export function loadHead(url = '/samples/manny_head.bin'): Promise<HeadShape | null> {
  pending ??= fetch(url)
    .then((r) => (r.ok ? r.arrayBuffer() : Promise.reject(new Error(String(r.status)))))
    .then(parseHead)
    .catch(() => null);
  return pending;
}

/** The head as a RootMesh, painted with `colorAt` per vertex (sRGB 0 to 1). */
export function headRootMesh(head: HeadShape, colorAt: (x: number, y: number, z: number, out: number[]) => void): RootMesh {
  const { positions, indices } = head.mesh;
  const colors = new Float32Array(positions.length);
  const c = [0, 0, 0];
  for (let v = 0; v < positions.length; v += 3) {
    colorAt(positions[v], positions[v + 1], positions[v + 2], c);
    colors[v] = c[0];
    colors[v + 1] = c[1];
    colors[v + 2] = c[2];
  }
  return { positions, indices, colors };
}

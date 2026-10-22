// SPDX-License-Identifier: Apache-2.0
/*
 * The head the landing page's grooms stand on while the engine loads: the Manny sample head ("Manny
 * head by Nick Rutlinh (GroomLab), CC BY 4.0"), whose subdivision cage
 * (web/public/samples/manny_head.bin, written by tools/web/make_head_mesh.py) is smoothed here with
 * Catmull-Clark.
 */
import type { RootMesh } from './strand-data';

export interface HeadShape {
  /** The head's surface as a mesh (y up, facing +z, the cranium's center at the origin). */
  mesh: { positions: Float32Array; indices: Uint32Array };
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
  return { mesh: { positions: pos, indices } };
}

let pending: Promise<HeadShape | null> | null = null;

/** The Manny head, fetched once per page; null when it can't be had. */
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

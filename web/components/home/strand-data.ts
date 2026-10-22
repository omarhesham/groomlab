// SPDX-License-Identifier: Apache-2.0
/*
 * What the 3D viewer draws, whoever makes it: the procedural groom in groom-3d.ts today, the WASM
 * engine later. Plain typed arrays, no three.js, so a source never has to know about the renderer.
 */

/** Strands as polylines, one after the other in one buffer. */
export interface StrandData {
  /** Number of strands. */
  count: number;
  /** Strand i's points are offsets[i] up to offsets[i + 1] - 1; at least count + 1 long. */
  offsets: Uint32Array;
  /** xyz per point (at least offsets[count] * 3 long). Y is up; the scalp sits around the origin. */
  positions: Float32Array;
  /** Optional sRGB color per point, rgb 0 to 1; grey when left out. */
  colors?: Float32Array;
}

/** The mesh the strands grow from, drawn as a smooth skin-neutral surface. */
export interface RootMesh {
  /** xyz per vertex. */
  positions: Float32Array;
  /** xyz per vertex; computed from the triangles when left out. */
  normals?: Float32Array;
  /** Three vertex indices per triangle. */
  indices: Uint32Array | Uint16Array;
  /** Optional sRGB color per vertex, rgb 0 to 1 (a scalp painted with the hair's root color). */
  colors?: Float32Array;
}

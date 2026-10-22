// SPDX-License-Identifier: Apache-2.0
/*
 * The Graph Editor's categories in the Playful theme's colors (hosts/maya/themes/playful.json, filled
 * headers): `hue` fills a node's header, `on` is the text on it, `tint` a light fill, `ink` a deeper
 * cut for lines and text on paper (AA).
 */

export type Category =
  | 'Sources'
  | 'Grow'
  | 'Shape'
  | 'Look'
  | 'Color'
  | 'Fields'
  | 'Utility'
  | 'Motion'
  | 'Feathers'
  | 'Convert'
  | 'Games'
  | 'Output';

export const category: Record<Category, { hue: string; on: string; tint: string; ink: string }> = {
  Sources: { hue: '#2ECC8F', on: '#0b3d2a', tint: '#c9f2e2', ink: '#16855b' },
  Grow: { hue: '#4FD1C5', on: '#0b3a36', tint: '#cdf3ef', ink: '#1f8a80' },
  Shape: { hue: '#2F7BF6', on: '#ffffff', tint: '#cfe0fd', ink: '#1d5fd0' },
  Look: { hue: '#FFC53D', on: '#3d2c00', tint: '#ffefc4', ink: '#a8780a' },
  Color: { hue: '#FF7AB6', on: '#4a0a28', tint: '#ffd9ea', ink: '#c9367a' },
  Fields: { hue: '#A36BFF', on: '#ffffff', tint: '#e6d8ff', ink: '#7440d6' },
  Utility: { hue: '#8A93A0', on: '#16191d', tint: '#e2e5e9', ink: '#59616c' },
  Motion: { hue: '#78C8FF', on: '#0a3352', tint: '#d9efff', ink: '#2683c4' },
  Feathers: { hue: '#FFC857', on: '#3d2c00', tint: '#fff0cc', ink: '#a8780a' },
  Convert: { hue: '#FF8A5C', on: '#4a1a06', tint: '#ffe0d3', ink: '#c9531f' },
  Games: { hue: '#5EC8FF', on: '#06324d', tint: '#d4efff', ink: '#1f86bf' },
  Output: { hue: '#1d1d1f', on: '#ffffff', tint: '#1d1d1f', ink: '#1d1d1f' },
};

/** Wire and port colors by data type (Playful's ports). */
export const port = {
  strands: '#FFC53D',
  guides: '#2F7BF6',
  mesh: '#2ECC8F',
  field: '#A36BFF',
  value: '#7CC8B4',
} as const;

export type PortType = keyof typeof port;

/** The mark's three strands. */
export const brand = { red: '#FF4D4F', yellow: '#FFC53D', blue: '#2F7BF6' } as const;

/** A cubic between two ports; `from`/`to` say which side of its node each port is on. */
export type Side = 'l' | 'r' | 't' | 'b';
const dir: Record<Side, [number, number]> = { l: [-1, 0], r: [1, 0], t: [0, -1], b: [0, 1] };

export function wirePath(x1: number, y1: number, x2: number, y2: number, from: Side = 'r', to: Side = 'l', pull?: number) {
  const horizontal = from === 'l' || from === 'r';
  const k = pull ?? Math.max(36, Math.abs(horizontal ? x2 - x1 : y2 - y1) * 0.5);
  const [ax, ay] = dir[from];
  const [bx, by] = dir[to];
  const r = (n: number) => Math.round(n * 10) / 10;
  return `M${r(x1)} ${r(y1)} C${r(x1 + ax * k)} ${r(y1 + ay * k)} ${r(x2 + bx * k)} ${r(y2 + by * k)} ${r(x2)} ${r(y2)}`;
}

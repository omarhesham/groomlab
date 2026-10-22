// SPDX-License-Identifier: Apache-2.0
/*
 * The Graph Editor's colors, for a light canvas. The category and port hues are the Charcoal theme's
 * (hosts/maya/themes/charcoal.json, graph.categories and graph.ports); `tint` is the header fill on
 * white and `ink` a deeper cut that reads as text on paper (AA).
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

export const category: Record<Category, { hue: string; tint: string; ink: string }> = {
  Sources: { hue: '#6DB897', tint: '#e6f3ec', ink: '#2c7656' },
  Grow: { hue: '#5EB0AA', tint: '#e2f2f1', ink: '#28726d' },
  Shape: { hue: '#7C9CD4', tint: '#e8eef9', ink: '#3a5ea3' },
  Look: { hue: '#D4B26A', tint: '#f8f0dc', ink: '#83641c' },
  Color: { hue: '#CF84A8', tint: '#f8e9f0', ink: '#a2406f' },
  Fields: { hue: '#A493D6', tint: '#efebf9', ink: '#6449b3' },
  Utility: { hue: '#8C8C8C', tint: '#efeff1', ink: '#55555a' },
  Motion: { hue: '#86AEDB', tint: '#e9f0f9', ink: '#3b6aa1' },
  Feathers: { hue: '#D6B877', tint: '#f8f1e1', ink: '#7f6424' },
  Convert: { hue: '#D48F6A', tint: '#f9ece4', ink: '#9a4f27' },
  Games: { hue: '#68B4D4', tint: '#e3f2f8', ink: '#25708f' },
  Output: { hue: '#1d1d1f', tint: '#1d1d1f', ink: '#1d1d1f' },
};

/** Wire and port colors by data type, a touch deeper than Charcoal's so they hold on white. */
export const port = {
  strands: '#cfa241',
  guides: '#6d8fd0',
  mesh: '#56ad86',
  field: '#9a86d3',
  value: '#5cb8a3',
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

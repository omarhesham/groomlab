// SPDX-License-Identifier: Apache-2.0
import type { CSSProperties, ReactNode } from 'react';
import { category, port as portColor, wirePath, type Category, type PortType, type Side } from './graph-theme';

/*
 * Graph Editor pieces drawn in SVG: node cards with a tinted header, monospace param rows and
 * ValueSlider-style bars, ports, and wires with pulses running along them (`.wire-pulse` in
 * global.css). Decoration only: whoever draws them hides the SVG from screen readers and states
 * the same thing in text.
 */

export const HEADER = 30;
export const ROW = 24;

export interface Row {
  label: string;
  value?: string;
  /** 0 to 1: draws a slider bar under the row. */
  slider?: number;
  /** Marks the row's slider and value (data-live) for a caller that moves them per frame. */
  live?: string;
}

export interface NodeSpec {
  id: string;
  name: string;
  cat: Category;
  x: number;
  y: number;
  w: number;
  rows?: Row[];
  /** Extra body height (for a viewport). */
  body?: number;
  ins?: { type: PortType; side?: Side; at?: number }[];
  outs?: { type: PortType; side?: Side; at?: number }[];
  mono?: boolean;
}

export function nodeHeight(n: NodeSpec) {
  return HEADER + (n.rows?.length ?? 0) * ROW + (n.rows?.length ? 8 : 0) + (n.body ?? 0);
}

/** Where a node's port is: inputs on the left (or `side`), outputs on the right, at header height or `at`. */
export function portPos(n: NodeSpec, kind: 'in' | 'out', i = 0): [number, number, Side] {
  const list = kind === 'in' ? n.ins : n.outs;
  const p = list?.[i];
  const side = p?.side ?? (kind === 'in' ? 'l' : 'r');
  const h = nodeHeight(n);
  if (side === 't') return [n.x + (p?.at ?? n.w / 2), n.y, side];
  if (side === 'b') return [n.x + (p?.at ?? n.w / 2), n.y + h, side];
  const y = n.y + (p?.at ?? HEADER / 2);
  return [side === 'l' ? n.x : n.x + n.w, y, side];
}

export function SvgNode({ n, children }: { n: NodeSpec; children?: ReactNode }) {
  const c = category[n.cat];
  const h = nodeHeight(n);
  const dark = n.cat === 'Output';
  const r = 10;
  const header = `M0 ${HEADER} V${r} Q0 0 ${r} 0 H${n.w - r} Q${n.w} 0 ${n.w} ${r} V${HEADER} Z`;
  return (
    <g transform={`translate(${n.x} ${n.y})`} className="graph-node">
      <rect x="0.5" y="0.5" width={n.w - 1} height={h - 1} rx={r} fill="#fff" stroke="#d9d9df" />
      <path d={header} fill={c.tint} />
      <line x1="0" x2={n.w} y1={HEADER} y2={HEADER} stroke={dark ? c.tint : '#e3e3e8'} />
      <circle cx="14" cy={HEADER / 2} r="4" fill={dark ? '#fff' : c.hue} />
      <text
        x="25"
        y={HEADER / 2 + 4.5}
        className={n.mono ? 'font-mono' : 'font-sans'}
        fontSize="12.5"
        fontWeight="600"
        fill={dark ? '#fff' : '#1d1d1f'}
      >
        {n.name}
      </text>
      {n.rows?.map((row, i) => {
        const y = HEADER + 6 + i * ROW;
        return (
          <g key={row.label} transform={`translate(0 ${y})`}>
            <text x="12" y="13" className="font-mono" fontSize="10.5" fill="#6e6e73">
              {row.label}
            </text>
            {row.value && (
              <text data-live={row.live && `${row.live}-value`} x={n.w - 12} y="13" textAnchor="end" className="font-mono" fontSize="10.5" fill="#1d1d1f">
                {row.value}
              </text>
            )}
            {row.slider !== undefined && (
              <>
                <rect x="12" y="18" width={n.w - 24} height="3" rx="1.5" fill="#ececf0" />
                <rect data-live={row.live && `${row.live}-slider`} x="12" y="18" width={(n.w - 24) * row.slider} height="3" rx="1.5" fill={c.hue} />
              </>
            )}
          </g>
        );
      })}
      {children}
      {n.ins?.map((_, i) => {
        const [x, y] = portPos(n, 'in', i);
        return <Port key={`i${i}`} x={x - n.x} y={y - n.y} color={portColor[n.ins![i].type]} />;
      })}
      {n.outs?.map((_, i) => {
        const [x, y] = portPos(n, 'out', i);
        return <Port key={`o${i}`} x={x - n.x} y={y - n.y} color={portColor[n.outs![i].type]} />;
      })}
    </g>
  );
}

export function Port({ x, y, color }: { x: number; y: number; color: string }) {
  return <circle cx={x} cy={y} r="4.5" fill={color} stroke="#fff" strokeWidth="2" className="graph-port" />;
}

/** A wire with a pulse travelling along it. `delay` staggers the pulses (seconds). */
export function Wire({
  d,
  color,
  delay = 0,
  dur = 2.8,
  draw = 0,
}: {
  d: string;
  color: string;
  delay?: number;
  dur?: number;
  /** When the wire draws in (seconds), where it does. */
  draw?: number;
}) {
  return (
    <g>
      <path
        d={d}
        pathLength={1}
        fill="none"
        stroke={color}
        strokeWidth="2"
        strokeOpacity="0.6"
        strokeLinecap="round"
        className="wire-draw"
        style={{ '--d': `${draw}s` } as CSSProperties}
      />
      <path
        d={d}
        pathLength={1}
        fill="none"
        stroke={color}
        strokeWidth="3.5"
        strokeLinecap="round"
        className="wire-pulse"
        style={{ '--pulse-delay': `${delay}s`, '--pulse-dur': `${dur}s` } as CSSProperties}
      />
    </g>
  );
}

/** A wire between two nodes' ports. */
export function connect(a: NodeSpec, ai: number, b: NodeSpec, bi: number, pull?: number) {
  const [x1, y1, s1] = portPos(a, 'out', ai);
  const [x2, y2, s2] = portPos(b, 'in', bi);
  return { d: wirePath(x1, y1, x2, y2, s1, s2, pull), color: portColor[a.outs?.[ai]?.type ?? 'strands'] };
}

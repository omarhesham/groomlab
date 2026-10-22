// SPDX-License-Identifier: Apache-2.0
'use client';
import { useCallback, useRef, useState, type CSSProperties } from 'react';
import type { GroomStats } from './groom-3d';
import { category, port, wirePath } from './graph-theme';
import type { StrandParams } from './strand-engine';
import { StrandViewport } from './strand-viewport';
import { connect, SvgNode, Wire, type NodeSpec } from './svg-graph';

/*
 * The hero: the groom itself, big and in 3D (a head turning slowly, its hair grown live), and under
 * it the small graph that makes it, Scalp to HairColor, pulses running along its wires and the last
 * one running up into the viewport's input. The Curl node's radius breathes on its own and the
 * curls follow. Two graph layouts: a wide row from 900px, a compact snake on phones.
 */

const params: StrandParams = {
  density: 240,
  clump: 0.72,
  clumpCount: 11,
  curl: 0.32,
  curlFrequency: 2,
  noise: 0.15,
  color: { melanin: 0.5, redness: 0.42 },
  showGuides: false,
  seed: 7,
};

const W = 124;
const wide: NodeSpec[] = [
  { id: 'scalp', name: 'Scalp', cat: 'Sources', x: 16, y: 54, w: W, rows: [{ label: 'mesh', value: 'head' }], outs: [{ type: 'mesh' }] },
  { id: 'guides', name: 'Guides', cat: 'Sources', x: 16, y: 170, w: W, rows: [{ label: 'level', value: '-1' }], outs: [{ type: 'guides' }] },
  {
    id: 'scatter',
    name: 'Scatter',
    cat: 'Grow',
    x: 214,
    y: 92,
    w: W,
    rows: [{ label: 'density', value: '120', slider: 0.55 }],
    ins: [{ type: 'mesh' }],
    outs: [{ type: 'strands' }],
  },
  {
    id: 'interpolate',
    name: 'Interpolate',
    cat: 'Grow',
    x: 412,
    y: 150,
    w: W,
    rows: [{ label: 'fillTubes', value: 'off' }],
    ins: [{ type: 'strands' }, { type: 'guides', at: 49 }],
    outs: [{ type: 'strands' }],
  },
  {
    id: 'clump',
    name: 'Clump',
    cat: 'Look',
    x: 610,
    y: 64,
    w: W,
    rows: [
      { label: 'strength', value: '0.72', slider: 0.72 },
      { label: 'count', value: '11' },
    ],
    ins: [{ type: 'strands' }],
    outs: [{ type: 'strands' }],
  },
  {
    id: 'curl',
    name: 'Curl',
    cat: 'Look',
    x: 808,
    y: 132,
    w: W,
    rows: [
      { label: 'radius', value: '0.32', slider: 0.32, live: 'curl-radius' },
      { label: 'frequency', value: '2.0' },
    ],
    ins: [{ type: 'strands' }],
    outs: [{ type: 'strands' }],
  },
  {
    id: 'color',
    name: 'HairColor',
    cat: 'Color',
    x: 1006,
    y: 72,
    w: W,
    rows: [
      { label: 'melanin', value: '0.50', slider: 0.5 },
      { label: 'redness', value: '0.42', slider: 0.42 },
    ],
    ins: [{ type: 'strands' }],
    outs: [{ type: 'strands', side: 't' }],
  },
];

const CW = 104;
// Phones: a snake, so every wire is short. The bottom row runs right, climbs, the top row runs
// back, and its last node feeds the viewport above.
const compact: NodeSpec[] = [
  { id: 'color', name: 'HairColor', cat: 'Color', x: 8, y: 34, w: CW, pill: true, ins: [{ type: 'strands', side: 'r' }], outs: [{ type: 'strands', side: 't' }] },
  { id: 'curl', name: 'Curl', cat: 'Look', x: 128, y: 34, w: CW, pill: true, ins: [{ type: 'strands', side: 'r' }], outs: [{ type: 'strands', side: 'l' }] },
  { id: 'clump', name: 'Clump', cat: 'Look', x: 248, y: 34, w: CW, pill: true, ins: [{ type: 'strands', side: 'b' }], outs: [{ type: 'strands', side: 'l' }] },
  { id: 'scalp', name: 'Scalp', cat: 'Sources', x: 8, y: 102, w: CW, pill: true, outs: [{ type: 'mesh' }] },
  { id: 'scatter', name: 'Scatter', cat: 'Grow', x: 128, y: 102, w: CW, pill: true, ins: [{ type: 'mesh' }], outs: [{ type: 'strands' }] },
  { id: 'interpolate', name: 'Interpolate', cat: 'Grow', x: 248, y: 102, w: CW, pill: true, ins: [{ type: 'strands' }], outs: [{ type: 'strands', side: 't' }] },
];

function chain(nodes: NodeSpec[]) {
  const by = Object.fromEntries(nodes.map((n) => [n.id, n]));
  const links: [string, number, string, number][] = [
    ['scalp', 0, 'scatter', 0],
    ['scatter', 0, 'interpolate', 0],
    ['guides', 0, 'interpolate', 1],
    ['interpolate', 0, 'clump', 0],
    ['clump', 0, 'curl', 0],
    ['curl', 0, 'color', 0],
  ];
  return links
    .filter(([a, , b]) => by[a] && by[b])
    .map(([a, ai, b, bi], i) => ({ key: `${a}-${b}`, i, ...connect(by[a], ai, by[b], bi) }));
}

/** Where the last node's wire meets the viewport's bottom edge (graph x, in viewBox units). */
function outputX(nodes: NodeSpec[]) {
  const c = nodes.find((n) => n.id === 'color')!;
  return c.x + c.w / 2;
}

function Graph({ nodes, vbw, vbh, className }: { nodes: NodeSpec[]; vbw: number; vbh: number; className?: string }) {
  const links = chain(nodes);
  const c = nodes.find((n) => n.id === 'color')!;
  const ox = outputX(nodes);
  return (
    <div className={className}>
      <div className="relative" style={{ aspectRatio: `${vbw} / ${vbh}` }}>
        <svg aria-hidden="true" viewBox={`0 0 ${vbw} ${vbh}`} className="graph-intro absolute inset-0 size-full overflow-visible">
          {links.map((w) => (
            <Wire key={w.key} d={w.d} color={w.color} delay={w.i * 0.4} draw={0.35 + w.i * 0.09} />
          ))}
          {/* HairColor's strands, up into the viewport. */}
          <Wire d={wirePath(ox, c.y, ox, 0, 't', 'b', 12)} color={port.strands} delay={links.length * 0.4} draw={0.35 + links.length * 0.09} />
          {nodes.map((n, i) => (
            <g key={n.id} className="node-in" style={{ '--d': `${0.1 + i * 0.07}s` } as CSSProperties}>
              <SvgNode n={n} />
            </g>
          ))}
        </svg>
      </div>
    </div>
  );
}

/** The Output node as a viewport: a header strip, the 3D groom, and an input port on its bottom edge. */
function OutputViewport({ live }: { live: (t: number) => Partial<StrandParams> }) {
  const [count, setCount] = useState<number | null>(null);
  const onStats = useCallback((s: GroomStats) => setCount(s.strands), []);
  const pct = (n: NodeSpec[], vbw: number) => `${(outputX(n) / vbw) * 100}%`;
  return (
    <div className="relative">
      <div className="overflow-hidden rounded-[18px] border border-[#e4e4e9] bg-[#f6f6f8]">
        <div className="flex h-10 items-center justify-between gap-3 border-b border-[#ebebef] px-3.5 font-mono text-[11.5px] text-gl-ink-2">
          <span className="flex items-center gap-2">
            <span aria-hidden="true" className="h-4 rounded-full px-2 text-[10.5px] leading-4 font-semibold text-white" style={{ background: category.Output.hue }}>
              Output
            </span>
            <span>
              <span className="text-gl-ink">head.groom</span> · persp
            </span>
          </span>
          <span className="flex items-center gap-2 tabular-nums">
            {count !== null && <span>{count.toLocaleString('en-US')} strands</span>}
            <span aria-hidden="true" className="live-dot size-1.5 rounded-full bg-gl-emerald" />
            live
          </span>
        </div>
        <div className="h-[min(112vw,440px)] sm:h-[clamp(420px,46vw,600px)]">
          <StrandViewport
            params={params}
            live={live}
            strandScale={12}
            spin={0.2}
            pitch={0.26}
            fill={1.02}
            onStats={onStats}
            label="A head of hair in 3D, turning slowly; drag to turn it"
          />
        </div>
      </div>
      {/* The input the graph's last wire runs into. */}
      <span
        aria-hidden="true"
        className="absolute -bottom-[6px] hidden size-[12px] -translate-x-1/2 rounded-full border-2 border-white min-[900px]:block"
        style={{ left: pct(wide, 1240), background: port.strands }}
      />
      <span
        aria-hidden="true"
        className="absolute -bottom-[6px] size-[12px] -translate-x-1/2 rounded-full border-2 border-white min-[900px]:hidden"
        style={{ left: pct(compact, 360), background: port.strands }}
      />
    </div>
  );
}

export function HeroGraph() {
  const rootRef = useRef<HTMLDivElement>(null);
  const live = useCallback((t: number) => {
    // Curl's radius breathes between 0.06 and 0.54; the node's slider and value follow it.
    const curl = 0.3 - 0.24 * Math.cos(t * 0.5);
    const root = rootRef.current;
    root?.querySelector('[data-live="curl-radius-slider"]')?.setAttribute('width', String((W - 24) * curl));
    const text = root?.querySelector('[data-live="curl-radius-value"]');
    if (text) text.textContent = curl.toFixed(2);
    return { curl };
  }, []);
  return (
    <div ref={rootRef}>
      <OutputViewport live={live} />
      <Graph nodes={wide} vbw={1240} vbh={260} className="hidden min-[900px]:block" />
      <Graph nodes={compact} vbw={360} vbh={150} className="min-[900px]:hidden" />
    </div>
  );
}

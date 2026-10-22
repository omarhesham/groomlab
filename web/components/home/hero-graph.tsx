// SPDX-License-Identifier: Apache-2.0
'use client';
import { useCallback, useRef, type CSSProperties } from 'react';
import type { StrandParams } from './strand-engine';
import { StrandViewport } from './strand-viewport';
import { connect, HEADER, nodeHeight, SvgNode, Wire, type NodeSpec } from './svg-graph';

/*
 * The hero: a small, live graph, Scalp to Output, with pulses running along its wires and the
 * groom it makes growing in the Output node's viewport. The Curl node's radius breathes on its
 * own and the strands follow. Two layouts: a wide graph from 900px, a compact one on phones.
 */

const params: StrandParams = {
  density: 240,
  clump: 0.72,
  clumpCount: 11,
  curl: 0.45,
  curlFrequency: 3,
  noise: 0.15,
  color: 'mark',
  showGuides: false,
  seed: 7,
};

const W = 124;
const wide: NodeSpec[] = [
  { id: 'scalp', name: 'Scalp', cat: 'Sources', x: 16, y: 58, w: W, rows: [{ label: 'mesh', value: 'head' }], outs: [{ type: 'mesh' }] },
  { id: 'guides', name: 'Guides', cat: 'Sources', x: 16, y: 266, w: W, rows: [{ label: 'level', value: '-1' }], outs: [{ type: 'guides' }] },
  {
    id: 'scatter',
    name: 'Scatter',
    cat: 'Grow',
    x: 168,
    y: 132,
    w: W,
    rows: [{ label: 'density', value: '120', slider: 0.55 }],
    ins: [{ type: 'mesh' }],
    outs: [{ type: 'strands' }],
  },
  {
    id: 'interpolate',
    name: 'Interpolate',
    cat: 'Grow',
    x: 320,
    y: 220,
    w: W,
    rows: [{ label: 'fillTubes', value: 'off' }],
    ins: [{ type: 'strands' }, { type: 'guides', at: 49 }],
    outs: [{ type: 'strands' }],
  },
  {
    id: 'clump',
    name: 'Clump',
    cat: 'Look',
    x: 472,
    y: 84,
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
    x: 624,
    y: 206,
    w: W,
    rows: [
      { label: 'radius', value: '0.45', slider: 0.45, live: 'curl-radius' },
      { label: 'frequency', value: '3.0' },
    ],
    ins: [{ type: 'strands' }],
    outs: [{ type: 'strands' }],
  },
  {
    id: 'color',
    name: 'HairColor',
    cat: 'Color',
    x: 776,
    y: 100,
    w: W,
    rows: [{ label: 'mode', value: 'color' }],
    ins: [{ type: 'strands' }],
    outs: [{ type: 'strands' }],
  },
  { id: 'output', name: 'Output', cat: 'Output', x: 932, y: 20, w: 292, body: 372, ins: [{ type: 'strands' }] },
];

const CW = 104;
// Phones: a snake, so every wire is short. Row one runs right, drops, row two runs back, and the
// last node drops into the Output.
const compact: NodeSpec[] = [
  { id: 'scalp', name: 'Scalp', cat: 'Sources', x: 8, y: 12, w: CW, outs: [{ type: 'mesh' }] },
  { id: 'scatter', name: 'Scatter', cat: 'Grow', x: 128, y: 12, w: CW, ins: [{ type: 'mesh' }], outs: [{ type: 'strands' }] },
  { id: 'interpolate', name: 'Interpolate', cat: 'Grow', x: 248, y: 12, w: CW, ins: [{ type: 'strands' }], outs: [{ type: 'strands', side: 'b' }] },
  { id: 'clump', name: 'Clump', cat: 'Look', x: 248, y: 80, w: CW, ins: [{ type: 'strands', side: 't' }], outs: [{ type: 'strands', side: 'l' }] },
  { id: 'curl', name: 'Curl', cat: 'Look', x: 128, y: 80, w: CW, ins: [{ type: 'strands', side: 'r' }], outs: [{ type: 'strands', side: 'l' }] },
  { id: 'color', name: 'HairColor', cat: 'Color', x: 8, y: 80, w: CW, ins: [{ type: 'strands', side: 'r' }], outs: [{ type: 'strands', side: 'b' }] },
  { id: 'output', name: 'Output', cat: 'Output', x: 8, y: 150, w: 344, body: 392, ins: [{ type: 'strands', side: 't', at: 52 }] },
];

function chain(nodes: NodeSpec[], pulls: Record<string, number> = {}) {
  const by = Object.fromEntries(nodes.map((n) => [n.id, n]));
  const links: [string, number, string, number][] = [
    ['scalp', 0, 'scatter', 0],
    ['scatter', 0, 'interpolate', 0],
    ['guides', 0, 'interpolate', 1],
    ['interpolate', 0, 'clump', 0],
    ['clump', 0, 'curl', 0],
    ['curl', 0, 'color', 0],
    ['color', 0, 'output', 0],
  ];
  return links
    .filter(([a, , b]) => by[a] && by[b])
    .map(([a, ai, b, bi], i) => ({ key: `${a}-${b}`, i, ...connect(by[a], ai, by[b], bi, pulls[`${a}-${b}`]) }));
}

/** The Output node's viewport, inside its body, leaving a footer line for the strand count. */
function viewport(n: NodeSpec) {
  return { x: n.x + 6, y: n.y + HEADER + 6, w: n.w - 12, h: nodeHeight(n) - HEADER - 34 };
}

function Graph({
  nodes,
  vbw,
  vbh,
  pulls,
  live,
  className,
}: {
  nodes: NodeSpec[];
  vbw: number;
  vbh: number;
  pulls?: Record<string, number>;
  live: (t: number) => Partial<StrandParams>;
  className?: string;
}) {
  const out = nodes.find((n) => n.id === 'output')!;
  const vp = viewport(out);
  const pct = (v: number, of: number) => `${(v / of) * 100}%`;
  return (
    <div className={className}>
      <div className="relative" style={{ aspectRatio: `${vbw} / ${vbh}` }}>
        <svg aria-hidden="true" viewBox={`0 0 ${vbw} ${vbh}`} className="graph-intro absolute inset-0 size-full overflow-visible">
          {chain(nodes, pulls).map((w) => (
            <Wire key={w.key} d={w.d} color={w.color} delay={w.i * 0.4} draw={0.35 + w.i * 0.09} />
          ))}
          {nodes.map((n, i) => (
            <g key={n.id} className="node-in" style={{ '--d': `${0.1 + i * 0.07}s` } as CSSProperties}>
              <SvgNode n={n}>
                {n.id === 'output' && (
                  <>
                    <rect x={vp.x - n.x} y={vp.y - n.y} width={vp.w} height={vp.h} rx="6" fill="#fafafb" stroke="#ebebef" />
                    <text x="12" y={nodeHeight(n) - 11} className="font-mono" fontSize="10.5" fill="#6e6e73">
                      persp
                    </text>
                    <text x={n.w - 12} y={nodeHeight(n) - 11} textAnchor="end" className="font-mono" fontSize="10.5" fill="#6e6e73">
                      {params.density} strands · live
                    </text>
                  </>
                )}
              </SvgNode>
            </g>
          ))}
        </svg>
        <div
          className="absolute overflow-hidden rounded-[6px]"
          style={{ left: pct(vp.x, vbw), top: pct(vp.y, vbh), width: pct(vp.w, vbw), height: pct(vp.h, vbh) }}
        >
          <StrandViewport params={params} live={live} />
        </div>
      </div>
    </div>
  );
}

export function HeroGraph() {
  const rootRef = useRef<HTMLDivElement>(null);
  const live = useCallback((t: number) => {
    // Curl's radius breathes between 0.15 and 0.75; the node's slider and value follow it.
    const curl = 0.45 - 0.3 * Math.cos(t * 0.55);
    const root = rootRef.current;
    root?.querySelector('[data-live="curl-radius-slider"]')?.setAttribute('width', String((W - 24) * curl));
    const text = root?.querySelector('[data-live="curl-radius-value"]');
    if (text) text.textContent = curl.toFixed(2);
    return { curl };
  }, []);
  return (
    <div ref={rootRef}>
      <Graph nodes={wide} vbw={1240} vbh={420} live={live} className="hidden min-[900px]:block" />
      <Graph nodes={compact} vbw={360} vbh={572} live={live} className="min-[900px]:hidden" />
    </div>
  );
}

// SPDX-License-Identifier: Apache-2.0
'use client';
import { Eye, RotateCcw, Shuffle } from 'lucide-react';
import { useMemo, useState, type CSSProperties, type ReactNode } from 'react';
import { LogoMark } from '@/components/logo';
import { Reveal } from '@/components/reveal';
import { cn } from '@/lib/cn';
import type { GroomStats } from './groom-3d';
import { category, port, surface } from './graph-theme';
import type { StrandParams } from './strand-engine';
import { StrandViewport } from './strand-viewport';
import { connect, SvgNode, Wire, type NodeSpec } from './svg-graph';

/*
 * "Made to be played with": three pillars (shape, style, detail) and a small GroomLab window: the
 * graph that grows a short curly cut on the Manny head, the bust in 3D beside it, and a toolbar
 * whose slider is the Curl node's radius. Guides, a new seed and a reset are a click away.
 */

const pillars: { title: string; text: string; icon: ReactNode }[] = [
  {
    title: 'Shape',
    text: 'Comb the guides with brushes, or shape whole locks with tubes. Short and clean to long and wild.',
    icon: (
      <svg viewBox="0 0 40 40" fill="none" stroke="currentColor" strokeWidth="2.4" strokeLinecap="round">
        <path d="M8 35c0-10 3-19 12-27" />
        <path d="M16 35c0-9 3-16 11-23" />
        <path d="M24 35c0-7 3-12 9-17" />
      </svg>
    ),
  },
  {
    title: 'Style',
    text: 'Clump, Curl, Noise and Braid are nodes. Stack them, bypass them, paint any of their parameters.',
    icon: (
      <svg viewBox="0 0 40 40" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round">
        <circle cx="20" cy="20" r="15" />
        <path d="M6 20h9M25 20h9" />
        <circle cx="20" cy="20" r="4.5" />
      </svg>
    ),
  },
  {
    title: 'Detail',
    text: 'Dial in density, frizz, flyaways and color, with every slider live on the strands.',
    icon: (
      <svg viewBox="0 0 40 40" fill="currentColor">
        <circle cx="20" cy="20" r="4.6" fill="none" stroke="currentColor" strokeWidth="2.2" />
        {Array.from({ length: 8 }, (_, i) => {
          const a = (i / 8) * Math.PI * 2;
          return <circle key={i} cx={20 + Math.cos(a) * 13} cy={20 + Math.sin(a) * 13} r="2.4" />;
        })}
      </svg>
    ),
  },
];

interface Look {
  curl: number;
  showGuides: boolean;
  seed: number;
}
const initial: Look = { curl: 0.55, showGuides: false, seed: 3 };

const W = 112;
function nodes(curl: number, strands: string): NodeSpec[] {
  return [
    { id: 'scalp', name: 'Scalp', cat: 'Sources', x: 4, y: 8, w: W, rows: [{ label: 'mesh', value: 'manny' }], outs: [{ type: 'mesh' }] },
    {
      id: 'scatter',
      name: 'Scatter',
      cat: 'Grow',
      x: 148,
      y: 28,
      w: W,
      rows: [{ label: 'density', value: '600', slider: 0.6 }],
      ins: [{ type: 'mesh' }],
      outs: [{ type: 'strands' }],
    },
    {
      id: 'clump',
      name: 'Clump',
      cat: 'Look',
      x: 292,
      y: 4,
      w: W,
      rows: [
        { label: 'strength', value: '0.62', slider: 0.62 },
        { label: 'count', value: '60' },
      ],
      ins: [{ type: 'strands' }],
      outs: [{ type: 'strands', side: 'b' }],
    },
    {
      id: 'curl',
      name: 'Curl',
      cat: 'Look',
      x: 222,
      y: 150,
      w: W,
      rows: [
        { label: 'radius', value: curl.toFixed(2), slider: curl },
        { label: 'frequency', value: '2.0' },
      ],
      ins: [{ type: 'strands', side: 't' }],
      outs: [{ type: 'strands', side: 'l' }],
    },
    {
      id: 'out',
      name: 'Output',
      cat: 'Output',
      x: 60,
      y: 168,
      w: W,
      rows: [{ label: 'strands', value: strands }],
      ins: [{ type: 'strands', side: 'r' }],
    },
  ];
}

function Graph({ curl, strands }: { curl: number; strands: string }) {
  const n = nodes(curl, strands);
  const by = Object.fromEntries(n.map((x) => [x.id, x]));
  const links: [string, string][] = [
    ['scalp', 'scatter'],
    ['scatter', 'clump'],
    ['clump', 'curl'],
    ['curl', 'out'],
  ];
  return (
    <svg aria-hidden="true" viewBox="0 0 408 262" className="graph-intro h-auto w-full max-w-[520px] overflow-visible">
      {links.map(([a, b], i) => {
        const w = connect(by[a], 0, by[b], 0);
        return <Wire key={a} d={w.d} color={w.color} delay={i * 0.5} draw={0.3 + i * 0.12} />;
      })}
      {n.map((x, i) => (
        <g key={x.id} className="node-in" style={{ '--d': `${i * 0.08}s` } as CSSProperties}>
          <SvgNode n={x} />
        </g>
      ))}
    </svg>
  );
}

/** Phones: the same chain as five name-only pills. */
function PillChain() {
  const names: [string, keyof typeof category][] = [
    ['Scalp', 'Sources'],
    ['Scatter', 'Grow'],
    ['Clump', 'Look'],
    ['Curl', 'Look'],
    ['Output', 'Output'],
  ];
  return (
    <svg aria-hidden="true" viewBox="0 0 360 30" className="h-auto w-full overflow-visible">
      {names.slice(1).map(([name], i) => (
        <path key={name} d={`M${i * 74 + 64} 15 H${(i + 1) * 74}`} stroke={i === 0 ? port.strands : port.strands} strokeWidth="2" />
      ))}
      {names.map(([name, cat], i) => (
        <g key={name} transform={`translate(${i * 74} 0)`}>
          <rect width="64" height="30" rx="15" fill={category[cat].hue} />
          <text x="32" y="19.5" textAnchor="middle" fontSize="12" fontWeight="600" fill={category[cat].on} className="font-sans">
            {name}
          </text>
        </g>
      ))}
    </svg>
  );
}

function ToolButton({ label, pressed, onClick, children }: { label: string; pressed?: boolean; onClick: () => void; children: ReactNode }) {
  return (
    <button
      type="button"
      aria-label={label}
      aria-pressed={pressed}
      title={label}
      onClick={onClick}
      className={cn(
        'grid size-9 shrink-0 place-items-center rounded-[9px] transition-colors focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-gl-blue',
        pressed ? 'bg-gl-blue-strong text-white' : 'bg-white/[0.05] text-gl-ink-2 ring-1 ring-white/[0.08] ring-inset hover:bg-white/[0.1] hover:text-gl-ink',
      )}
    >
      {children}
    </button>
  );
}

function AppPanel() {
  const [look, setLook] = useState<Look>(initial);
  const [stats, setStats] = useState<GroomStats | null>(null);
  const strands = stats ? stats.strands.toLocaleString('en-US') : '7,200';
  const params = useMemo<StrandParams>(
    () => ({
      density: 600,
      clump: 0.62,
      clumpCount: 60,
      curl: look.curl,
      curlFrequency: 2,
      noise: 0.15,
      color: { melanin: 0.56, redness: 0.42 },
      showGuides: look.showGuides,
      seed: look.seed,
      length: 1.25,
      volume: 0.35,
    }),
    [look],
  );
  return (
    <div className="overflow-hidden rounded-[14px] border border-white/[0.09] bg-gl-panel shadow-[0_30px_80px_-40px_rgba(0,0,0,0.9)]">
      {/* The title bar. */}
      <div className="flex h-11 items-center gap-2.5 border-b border-white/[0.07] px-4">
        <LogoMark className="size-5" />
        <span className="text-[13.5px] font-semibold text-gl-ink">GroomLab</span>
        <span aria-hidden="true" className="ml-auto flex gap-1.5">
          {[0, 1, 2].map((i) => (
            <span key={i} className="size-[7px] rounded-full bg-white/20" />
          ))}
        </span>
      </div>
      <div className="grid md:grid-cols-[minmax(0,1.6fr)_minmax(0,1fr)]">
        <div className="flex flex-col md:border-r md:border-white/[0.07]">
          <div className="hidden flex-1 items-center justify-center px-5 py-7 md:flex lg:px-7">
            <Graph curl={look.curl} strands={strands} />
          </div>
          <div className="px-4 pt-5 pb-1 md:hidden">
            <PillChain />
          </div>
          {/* The toolbar. */}
          <div className="order-last flex h-[58px] items-center gap-2 border-t border-white/[0.07] px-3 md:order-none">
            <ToolButton
              label="Show the guides"
              pressed={look.showGuides}
              onClick={() => setLook((l) => ({ ...l, showGuides: !l.showGuides }))}
            >
              <Eye className="size-4" strokeWidth={2} />
            </ToolButton>
            <ToolButton label="New random roots" onClick={() => setLook((l) => ({ ...l, seed: l.seed + 1 }))}>
              <Shuffle className="size-4" strokeWidth={2} />
            </ToolButton>
            <ToolButton label="Reset" onClick={() => setLook(initial)}>
              <RotateCcw className="size-4" strokeWidth={2} />
            </ToolButton>
            <label className="ml-auto flex items-center gap-3 text-[12px] whitespace-nowrap text-gl-ink-2">
              <span className="hidden font-mono sm:inline">curl radius</span>
              <span className="font-mono sm:hidden">curl</span>
              <span className="relative flex h-9 w-24 items-center sm:w-32 lg:w-24 xl:w-32">
                <span aria-hidden="true" className="absolute inset-x-0 h-[3px] rounded-full" style={{ background: surface.track }} />
                <span aria-hidden="true" className="absolute left-0 h-[3px] rounded-full bg-gl-blue" style={{ width: `${look.curl * 100}%` }} />
                <span
                  aria-hidden="true"
                  className="absolute size-3.5 -translate-x-1/2 rounded-full border-2 border-gl-panel bg-gl-blue shadow-[0_0_0_3px_rgba(47,123,246,0.25)]"
                  style={{ left: `${look.curl * 100}%` }}
                />
                <input
                  type="range"
                  min={0}
                  max={1}
                  step={0.01}
                  value={look.curl}
                  aria-valuetext={look.curl.toFixed(2)}
                  onChange={(e) => setLook((l) => ({ ...l, curl: Number(e.target.value) }))}
                  className="peer absolute inset-0 size-full cursor-ew-resize appearance-none opacity-0"
                />
              </span>
              <span className="w-8 text-right font-mono text-gl-ink tabular-nums">{look.curl.toFixed(2)}</span>
            </label>
          </div>
        </div>
        <div className="relative h-[300px] border-b border-white/[0.07] md:h-auto md:min-h-[340px] md:border-b-0">
          <StrandViewport
            params={params}
            onStats={setStats}
            strandScale={12}
            pitch={0.1}
            yaw={1.05}
            fill={1}
            skin={[0.7, 0.69, 0.68]}
            framing={[[0, -0.35, 0.12], 1.7]}
            label="The Manny head in 3D with a short curly cut, side on; drag to turn it"
          />
          <span className="pointer-events-none absolute right-3 bottom-2.5 font-mono text-[11px] text-gl-ink-3 tabular-nums">
            {strands} strands
          </span>
        </div>
      </div>
    </div>
  );
}

export function PlaySection() {
  return (
    <section id="play" aria-labelledby="play-title" className="relative">
      <div className="wrap-wide border-t border-white/[0.08] py-[clamp(4rem,2.5rem+4vw,6.5rem)]">
        <div className="grid items-center gap-12 lg:grid-cols-2 lg:gap-14">
          <Reveal>
            <h2 id="play-title" className="display t-section">
              Made to be played with.
            </h2>
            <ul className="mt-10 grid gap-8 sm:grid-cols-3 sm:gap-0">
              {pillars.map((p, i) => (
                <li key={p.title} className={cn('sm:pr-6', i > 0 && 'sm:border-l sm:border-white/[0.09] sm:pl-6')}>
                  <span aria-hidden="true" className="block size-10 text-gl-blue">
                    {p.icon}
                  </span>
                  <h3 className="mt-5 text-[21px] font-semibold tracking-tight text-gl-ink">{p.title}</h3>
                  <p className="mt-2.5 text-[15px] leading-relaxed text-gl-ink-2">{p.text}</p>
                </li>
              ))}
            </ul>
          </Reveal>
          <Reveal delay={120}>
            <AppPanel />
          </Reveal>
        </div>
      </div>
    </section>
  );
}

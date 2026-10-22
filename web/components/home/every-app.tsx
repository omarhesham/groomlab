// SPDX-License-Identifier: Apache-2.0
import Link from 'next/link';
import type { CSSProperties } from 'react';
import { Reveal } from '@/components/reveal';
import { category, port, wirePath, type Category, type Side } from './graph-theme';
import { SectionHead } from './section-node';
import { portPos, SvgNode, Wire, type NodeSpec } from './svg-graph';

/*
 * One groom, every app: the .groom node in the middle, the apps that groom it wired in on one side,
 * the renderers and engines it goes to fanned out on the other, pulses running through. Drawn
 * twice: across on wide screens, top to bottom on phones.
 */

const hosts = [
  { name: 'Maya', label: 'plug-in', value: '2022–2027' },
  { name: 'Blender', label: 'add-on', value: 'beta' },
  { name: 'GroomLab app', label: 'standalone', value: 'beta' },
];
const renderers = ['Arnold', 'V-Ray', 'RenderMan'];
const exportsTo = ['Unreal', 'Unity', 'glTF', 'Alembic', 'USD'];

const hostCat: Category = 'Sources';
const renderCat: Category = 'Fields';
const exportCat: Category = 'Games';

interface Layout {
  vbw: number;
  vbh: number;
  hosts: NodeSpec[];
  groom: NodeSpec;
  outs: NodeSpec[];
  labels: { x: number; y: number; text: string; anchor?: 'start' | 'end' | 'middle' }[];
  /** Route the outgoing wires down one trunk and off to each node (the narrow layout). */
  bus?: boolean;
}

function wide(): Layout {
  const groom: NodeSpec = {
    id: 'groom',
    name: 'wolf.groom',
    cat: 'Output',
    mono: true,
    x: 440,
    y: 180,
    w: 200,
    pill: true,
    ins: [{ type: 'mesh' }],
    outs: [{ type: 'strands' }],
  };
  const hostNodes: NodeSpec[] = hosts.map((h, i) => ({
    id: h.name,
    name: h.name,
    cat: hostCat,
    x: 0,
    y: 108 + i * 64,
    w: 180,
    pill: true,
    outs: [{ type: 'mesh' }],
  }));
  const outs: NodeSpec[] = [
    ...renderers.map((name, i) => ({ id: name, name, cat: renderCat, x: 900, y: 32 + i * 44, w: 180, pill: true, ins: [{ type: 'field' as const }] })),
    ...exportsTo.map((name, i) => ({ id: name, name, cat: exportCat, x: 900, y: 196 + i * 44, w: 180, pill: true, ins: [{ type: 'value' as const }] })),
  ];
  return {
    vbw: 1080,
    vbh: 420,
    hosts: hostNodes,
    groom,
    outs,
    labels: [
      { x: 900, y: 20, text: 'RENDER' },
      { x: 900, y: 184, text: 'EXPORT' },
      { x: 0, y: 94, text: 'GROOM IN' },
      { x: 540, y: 166, text: 'ONE FILE', anchor: 'middle' },
    ],
  };
}

function compact(): Layout {
  const groom: NodeSpec = {
    id: 'groom',
    name: 'wolf.groom',
    cat: 'Output',
    mono: true,
    x: 80,
    y: 150,
    w: 200,
    pill: true,
    ins: [{ type: 'mesh', side: 't' }],
    outs: [{ type: 'strands', side: 'b' }],
  };
  const hostNodes: NodeSpec[] = hosts.map((h, i) => ({
    id: h.name,
    name: h.name === 'GroomLab app' ? 'App' : h.name,
    cat: hostCat,
    x: i * 124,
    y: 14,
    w: 112,
    pill: true,
    outs: [{ type: 'mesh', side: 'b' }],
  }));
  const outs: NodeSpec[] = [
    ...renderers.map((name, i) => ({ id: name, name, cat: renderCat, x: 0, y: 270 + i * 44, w: 160, pill: true, ins: [{ type: 'field' as const, side: 'r' as Side }] })),
    ...exportsTo.map((name, i) => ({ id: name, name, cat: exportCat, x: 200, y: 270 + i * 44, w: 160, pill: true, ins: [{ type: 'value' as const, side: 'l' as Side }] })),
  ];
  return {
    vbw: 360,
    vbh: 490,
    hosts: hostNodes,
    groom,
    outs,
    labels: [
      { x: 0, y: 258, text: 'RENDER' },
      { x: 360, y: 258, text: 'EXPORT', anchor: 'end' },
    ],
    bus: true,
  };
}

function Diagram({ layout, className }: { layout: Layout; className?: string }) {
  const { groom } = layout;
  const hostWires = layout.hosts.map((h, i) => {
    const [x1, y1, s1] = portPos(h, 'out');
    const [x2, y2, s2] = portPos(groom, 'in');
    return wirePath(x1, y1, x2, y2, s1, s2);
  });
  const [ox, oy, os] = portPos(groom, 'out');
  const outWires = layout.outs.map((n) => {
    const [x2, y2, s2] = portPos(n, 'in');
    const d = layout.bus
      ? `M${ox} ${oy} V${y2 - 10} Q${ox} ${y2} ${ox + (x2 < ox ? -10 : 10)} ${y2} H${x2}`
      : wirePath(ox, oy, x2, y2, os, s2);
    return { d, color: category[n.cat].hue };
  });
  const nodes = [...layout.hosts, groom, ...layout.outs];
  return (
    <svg aria-hidden="true" viewBox={`0 0 ${layout.vbw} ${layout.vbh}`} className={className} overflow="visible">
      {hostWires.map((d, i) => (
        <Wire key={`h${i}`} d={d} color={port.mesh} delay={i * 0.5} dur={2.4} draw={0.1 + i * 0.08} />
      ))}
      {outWires.map((w, i) => (
        <Wire key={`o${i}`} d={w.d} color={w.color} delay={1.2 + i * 0.22} dur={2.4} draw={0.5 + i * 0.06} />
      ))}
      {layout.labels.map((l) => (
        <text
          key={l.text}
          x={l.x}
          y={l.y}
          textAnchor={l.anchor ?? 'start'}
          className="font-mono"
          fontSize="10.5"
          letterSpacing="0.08em"
          fill="#86868b"
        >
          {l.text}
        </text>
      ))}
      {nodes.map((n, i) => (
        <g key={n.id} className="node-in" style={{ '--d': `${0.05 + i * 0.04}s` } as CSSProperties}>
          <SvgNode n={n} />
        </g>
      ))}
      {/* The file's own mark on its node. */}
      <g transform={`translate(${groom.x + groom.w - 32} ${groom.y + 6})`}>
        <path d="M3 15 C3 9 5 5 9 3.5" stroke="#FF4D4F" strokeWidth="2" strokeLinecap="round" fill="none" />
        <path d="M7 15 C7 10 9 7 13 6" stroke="#FFC53D" strokeWidth="2" strokeLinecap="round" fill="none" />
        <path d="M11 15 C11 11 13 9 16 8.5" stroke="#2F7BF6" strokeWidth="2" strokeLinecap="round" fill="none" />
      </g>
    </svg>
  );
}

const tags = ['Identical everywhere', 'Tiny files', 'Always editable'];

export function EveryApp() {
  return (
    <section aria-labelledby="apps-title" className="section-y">
      <div className="wrap">
        <SectionHead
          id="apps-title"
          cat="Sources"
          index="02"
          tag="One groom"
          title="One groom. Every app."
          muted="The same hair."
          lede={
            <p>
              A <code className="font-mono text-[0.9em] text-gl-ink">.groom</code> file holds the scalp, the guides, the
              maps and the graph. Maya, Blender and the GroomLab app open it as it is, and it renders and exports the
              same everywhere.
            </p>
          }
        />

        <Reveal className="mt-14 sm:mt-20">
          <Diagram layout={wide()} className="hidden h-auto w-full md:block" />
          <Diagram layout={compact()} className="mx-auto block h-auto w-full max-w-[420px] md:hidden" />
          <div className="sr-only">
            <p>Groom in: {hosts.map((h) => h.name).join(', ')}.</p>
            <p>Render in: {renderers.join(', ')}.</p>
            <p>Export to: {exportsTo.join(', ')}.</p>
          </div>
        </Reveal>

        <Reveal className="mt-14 grid gap-8 border-t border-[#e4e4e9] pt-8 sm:mt-16 lg:grid-cols-12">
          <ul className="flex flex-wrap content-start items-start gap-2 lg:col-span-5">
            {tags.map((t) => (
              <li key={t} className="rounded-full border border-[#dcdce1] bg-white px-3.5 py-1.5 text-[14px] font-medium">
                {t}
              </li>
            ))}
          </ul>
          <p className="text-[15px] leading-relaxed text-gl-ink-2 lg:col-span-7">
            V-Ray: native V-Ray scenes, rendered in V-Ray Standalone and V-Ray for Blender (V-Ray for Maya 2024+
            experimental). Redshift and others: through Alembic and USD exports.{' '}
            <Link href="/docs/other-renderers" className="font-medium text-gl-ink underline underline-offset-4">
              See how
            </Link>
          </p>
        </Reveal>
      </div>
    </section>
  );
}

// SPDX-License-Identifier: Apache-2.0
import type { CSSProperties, ReactNode } from 'react';
import { Reveal } from '@/components/reveal';
import { TextLink } from '@/components/text-link';
import { cn } from '@/lib/cn';
import { accentColor, accentText, type Accent } from './feature-section';
import { Curl } from './strands';

/* Every node in the graph, by category (see content/docs/node-graph.mdx). */
const categories: { name: string; accent: Accent; nodes: string[] }[] = [
  { name: 'Sources', accent: 'blue', nodes: ['Scalp', 'Guides', 'ColliderMesh', 'ImportCurves', 'Reference'] },
  { name: 'Grow', accent: 'emerald', nodes: ['Scatter', 'Interpolate', 'Cull'] },
  {
    name: 'Shape',
    accent: 'yellow',
    nodes: [
      'Length',
      'Width',
      'Resample',
      'Sculpt',
      'Direction',
      'Bend',
      'Transform',
      'Flow',
      'Part',
      'Tie',
      'Smooth',
      'BlendNeighbors',
      'Gravity',
      'Displace',
      'Mirror',
      'Collide',
      'SelfCollide',
      'MeshCut',
      'RestoreLength',
    ],
  },
  {
    name: 'Look',
    accent: 'red',
    nodes: ['Clump', 'Wet', 'Twist', 'Braid', 'Noise', 'Curl', 'Flyaway', 'Static', 'Tangle', 'Instance', 'Scales'],
  },
  { name: 'Motion', accent: 'purple', nodes: ['Wind', 'Jiggle', 'Simulate'] },
  { name: 'Feathers', accent: 'blue', nodes: ['Propagate', 'Plume', 'Split'] },
  { name: 'Color', accent: 'red', nodes: ['HairColor', 'Bands', 'FeatherColor', 'RenderData'] },
  { name: 'Convert', accent: 'emerald', nodes: ['ToGuides', 'ToStrands'] },
  {
    name: 'Fields & Curves',
    accent: 'yellow',
    nodes: ['Map', 'Expression', 'Control', 'FieldNoise', 'FieldRemap', 'FieldCombine', 'Curve'],
  },
  { name: 'Utility', accent: 'purple', nodes: ['Group', 'Blend', 'Attribute', 'Merge', 'Switch', 'Output'] },
  { name: 'Games', accent: 'red', nodes: ['Cards'] },
];

const nodes = categories.flatMap((c) => c.nodes.map((n) => ({ name: n, accent: c.accent })));
const half = Math.ceil(nodes.length / 2);
const rows = [nodes.slice(0, half), nodes.slice(half)];

const specs: { title: string; accent: Accent }[] = [
  { title: 'A procedural for the farm', accent: 'blue' },
  { title: 'The same groom on every machine', accent: 'emerald' },
  { title: 'Maps and keys on any parameter', accent: 'yellow' },
  { title: 'Python API and a headless CLI', accent: 'purple' },
];

/* From the Python API docs (content/docs/python-api.mdx). */
const code = `import groomlab
from groomlab import graph

groom = groomlab.create_groom("head")

model = graph.GraphModel.from_groom(groom)
model.add_node("Curl", name="curl")
model.insert_between("noise", "color", "curl")
model.set_param("curl", "radius", 0.08)
model.save(groom)   # undoable

groomlab.export_alembic(groom, "C:/cache/groom.abc",
                        start=1, end=48)`;

export function UnderTheHood() {
  return (
    <section aria-labelledby="hood-title" className="section-y overflow-x-clip pb-4! sm:pb-8!">
      <div className="wrap">
        <Reveal className="mx-auto max-w-3xl text-center">
          <p className={cn('t-eyebrow flex items-center justify-center gap-2', accentText.purple)}>
            <Curl color={accentColor.purple} className="size-5" />
            Under the hood
          </p>
          <h2 id="hood-title" className="display t-section mt-4">
            Built for groom artists. <span className="text-gl-ink-3">Down to the strand.</span>
          </h2>
        </Reveal>
      </div>

      <Reveal className="mt-14 space-y-3 sm:mt-20 sm:space-y-4">
        <p className="sr-only">GroomLab&apos;s nodes: {nodes.map((n) => n.name).join(', ')}.</p>
        {rows.map((row, i) => (
          <NodeRow key={i} nodes={row} reverse={i === 1} duration={i === 0 ? 80 : 74} />
        ))}
      </Reveal>
      <div className="wrap mt-6 flex justify-center">
        <TextLink href="/docs/node-graph">Every node, explained</TextLink>
      </div>

      <div className="wrap mt-20 grid grid-cols-[minmax(0,1fr)] gap-12 sm:mt-28 lg:grid-cols-12 lg:gap-16">
        <ul className="grid content-center gap-4 sm:grid-cols-2 lg:col-span-7 lg:grid-cols-1">
          {specs.map((s, i) => (
            <Reveal key={s.title} delay={(i % 2) * 90}>
              <li className="flex items-center gap-3 rounded-[18px] border border-black/[0.06] bg-white px-5 py-4 text-[17px] font-semibold tracking-tight">
                <span aria-hidden="true" className="size-2.5 shrink-0 rounded-full" style={{ background: accentColor[s.accent] }} />
                {s.title}
              </li>
            </Reveal>
          ))}
        </ul>
        <Reveal delay={120} className="lg:col-span-5">
          <CodeCard name="make_groom.py" code={code} />
          <TextLink href="/docs/python-api" className="mt-5">
            The Python API
          </TextLink>
        </Reveal>
      </div>
    </section>
  );
}

/**
 * A row of node chips, wired together like the graph, drifting sideways.
 * The chips are drawn twice so the loop is seamless; for readers who ask for
 * reduced motion the row stands still and wraps instead.
 */
function NodeRow({
  nodes: row,
  reverse,
  duration,
}: {
  nodes: { name: string; accent: Accent }[];
  reverse?: boolean;
  duration: number;
}) {
  const chips = (copy: number) =>
    row.map((n) => (
      <li key={`${copy}-${n.name}`} className={cn(copy > 0 && 'marquee-copy')}>
        <span className="relative flex items-center gap-2 rounded-full border border-black/[0.08] bg-white py-2 pr-4 pl-3 font-mono text-[13px] font-medium text-gl-ink sm:text-sm">
          <span aria-hidden="true" className="size-2 rounded-full" style={{ background: accentColor[n.accent] }} />
          {n.name}
        </span>
      </li>
    ));
  return (
    <div aria-hidden="true" className="marquee-row relative">
      {/* The wire the chips sit on. */}
      <span className="marquee-wire absolute inset-x-0 top-1/2 h-px bg-black/[0.08]" />
      <ul
        className={cn('marquee relative flex', reverse && 'marquee-reverse')}
        style={{ '--marquee-dur': `${duration}s` } as CSSProperties}
      >
        {chips(0)}
        {chips(1)}
      </ul>
    </div>
  );
}

/** A dark editor card with the Python lightly highlighted. */
function CodeCard({ name, code: source }: { name: string; code: string }) {
  return (
    <figure className="overflow-hidden rounded-[22px] bg-gl-night text-gl-white [color-scheme:dark]">
      <figcaption className="flex items-center gap-2 border-b border-white/[0.08] px-5 py-3.5">
        <span aria-hidden="true" className="size-3 rounded-full bg-gl-red" />
        <span aria-hidden="true" className="size-3 rounded-full bg-gl-yellow" />
        <span aria-hidden="true" className="size-3 rounded-full bg-gl-emerald" />
        <span className="ml-2 font-mono text-[13px] text-gl-dim">{name}</span>
      </figcaption>
      <pre className="overflow-x-auto px-5 py-5 font-mono text-[12.5px] leading-[1.7] sm:text-[13px]">
        <code>
          {source.split('\n').map((line, i) => (
            <span key={i} className="block min-h-[1lh]">
              {highlight(line)}
            </span>
          ))}
        </code>
      </pre>
    </figure>
  );
}

const TOKEN = /(#.*$)|("[^"]*")|\b(import|from)\b|\b(\d+(?:\.\d+)?)\b|(\w+)(?=\()/g;

function highlight(line: string): ReactNode[] {
  const out: ReactNode[] = [];
  let last = 0;
  for (const m of line.matchAll(TOKEN)) {
    if (m.index > last) out.push(line.slice(last, m.index));
    const color = m[1]
      ? 'text-gl-dim/70'
      : m[2]
        ? 'text-[#7ee2b8]'
        : m[3]
          ? 'text-[#c9a6ff]'
          : m[4]
            ? 'text-gl-yellow'
            : 'text-gl-blue-soft';
    out.push(
      <span key={m.index} className={color}>
        {m[0]}
      </span>,
    );
    last = m.index + m[0].length;
  }
  if (last < line.length) out.push(line.slice(last));
  return out;
}

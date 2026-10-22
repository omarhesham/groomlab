// SPDX-License-Identifier: Apache-2.0
import Link from 'next/link';
import type { ReactNode } from 'react';
import { Reveal } from '@/components/reveal';
import { category, type Category } from './graph-theme';
import { SectionHead } from './section-node';
import { BrushArt, CardsArt, FeathersArt, PaintArt, SimulateArt, TubeArt } from './toolkit-art';

/*
 * The toolkit, laid out like the Graph Editor's node palette: each tool is a node card with its
 * category's header, its ports, a small scene drawn in code, and where to read more.
 */

const tools: { cat: Category; node: string; title: string; text: string; href: string; art: ReactNode }[] = [
  {
    cat: 'Shape',
    node: 'Groom by hand',
    title: 'Brush the guides. The hair follows.',
    text: 'Comb the guides with brushes and the hair follows, live.',
    href: '/docs/guides-and-brushes',
    art: <BrushArt />,
  },
  {
    cat: 'Fields',
    node: 'Paint anything',
    title: 'Paint any parameter.',
    text: 'Right-click any parameter and paint it on the scalp.',
    href: '/docs/painting-maps',
    art: <PaintArt />,
  },
  {
    cat: 'Grow',
    node: 'Tube guides',
    title: 'Shape whole locks.',
    text: 'Every guide has a tube. Shape it and its strands stay inside to the tip: anime hair, locks and spikes with no mesh to model.',
    href: '/docs/guides-and-brushes#tube-guides',
    art: <TubeArt />,
  },
  {
    cat: 'Games',
    node: 'Cards',
    title: 'From groom to game engine.',
    text: 'Cards baked from your own strands, with LODs and texture maps, ready for Unreal, Unity and glTF.',
    href: '/docs/game-export',
    art: <CardsArt />,
  },
  {
    cat: 'Motion',
    node: 'Simulate',
    title: 'Hair that moves.',
    text: 'Wind and Jiggle for procedural motion, the Simulate node for the guides, and grooms that follow the scalp.',
    href: '/docs/motion-and-simulation',
    art: <SimulateArt />,
  },
  {
    cat: 'Feathers',
    node: 'Feathers',
    title: 'Fur, feathers and scales.',
    text: 'Feathers grown from shafts and layered with Plume, flow fields for short fur, and scales placed by strands.',
    href: '/docs/fur-and-feathers',
    art: <FeathersArt />,
  },
];

export function Toolkit() {
  return (
    <section id="features" aria-labelledby="features-title" className="section-y">
      <div className="wrap">
        <SectionHead
          id="features-title"
          cat="Look"
          index="03"
          tag="Toolkit"
          title="Everything a groom needs."
          muted="Nothing it doesn't."
          lede={<p>Guides you shape by hand, a graph that grows the strands from them, and the tools around both.</p>}
        />
        <ul className="mt-14 grid gap-5 sm:mt-16 sm:grid-cols-2 lg:grid-cols-3">
          {tools.map((t, i) => (
            <li key={t.node}>
              <Reveal delay={(i % 3) * 80} className="h-full">
                <ToolCard {...t} />
              </Reveal>
            </li>
          ))}
        </ul>
      </div>
    </section>
  );
}

function ToolCard({ cat, node, title, text, href, art }: (typeof tools)[number]) {
  const c = category[cat];
  return (
    <article className="tool-card group relative flex h-full flex-col">
      <div className="flex h-10 items-center gap-2.5 rounded-[12px] px-3.5" style={{ background: c.hue, color: c.on }}>
        <span aria-hidden="true" className="size-2 rounded-full bg-current opacity-85" />
        <span className="text-[13px] font-semibold">
          {node}
        </span>
        <span className="ml-auto font-mono text-[11px] opacity-75">{cat === 'Fields' ? 'Fields & Curves' : cat}</span>
      </div>
      {/* Ports: they swell toward the pointer when the card is hovered. */}
      <span aria-hidden="true" className="tool-port absolute top-5 -left-[5px]" style={{ background: c.hue }} />
      <span aria-hidden="true" className="tool-port absolute top-5 -right-[5px]" style={{ background: c.hue }} />
      <div className="art-canvas aspect-[16/10] border-b border-[#efeff2]">{art}</div>
      <div className="flex flex-1 flex-col p-5">
        <h3 className="text-[19px] font-semibold tracking-tight">{title}</h3>
        <p className="mt-2 flex-1 text-[15px] leading-relaxed text-gl-ink-2">{text}</p>
        <Link
          href={href}
          className="mt-4 inline-flex items-center gap-1 self-start rounded-md text-[15px] font-medium text-gl-blue-strong after:absolute after:inset-0 after:rounded-[16px] hover:underline hover:underline-offset-4 focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-gl-blue"
        >
          Learn more<span className="sr-only"> about {node.toLowerCase()}</span>
          <svg aria-hidden="true" viewBox="0 0 16 16" className="size-3.5 transition-transform duration-500 ease-spring group-hover:translate-x-1" fill="none">
            <path d="M6 3.5 10.5 8 6 12.5" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
          </svg>
        </Link>
      </div>
    </article>
  );
}

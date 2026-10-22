// SPDX-License-Identifier: Apache-2.0
import type { ReactNode } from 'react';
import { Reveal } from '@/components/reveal';
import { cn } from '@/lib/cn';

/*
 * How a groom is made, in words (the graph below is the hands-on part): an artist shapes a few
 * hundred guides, the graph grows every strand between them, and nothing is ever baked. Three
 * steps, shape, style and detail, each with its glyph.
 */

const steps: { title: string; text: string; icon: ReactNode }[] = [
  {
    title: 'Shape',
    text: 'Comb a few hundred guides with brushes, or block out whole locks as tubes. They are the only curves you touch.',
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
    text: 'The graph grows every strand between them, then clumps, curls and frizzes them. Each look is a node you can bypass or paint.',
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
    text: 'Density, flyaways, width and melanin are sliders on live strands, from a 100k preview to the millions you render.',
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

export function PlaySection() {
  return (
    <section id="how" aria-labelledby="how-title" className="relative">
      <div className="wrap-wide border-t border-white/[0.08] py-[clamp(4rem,2.5rem+4vw,6.5rem)]">
        <div className="grid gap-12 lg:grid-cols-[minmax(0,1fr)_minmax(0,1.15fr)] lg:gap-16">
          <Reveal>
            <h2 id="how-title" className="display t-section">
              Comb a few guides.
              <br />
              <span className="text-gl-ink-3">Grow a whole groom.</span>
            </h2>
            <p className="mt-6 max-w-[34rem] text-[17px] leading-relaxed text-gl-ink-2">
              You never place a hundred thousand hairs by hand. You shape the few that matter, and GroomLab fills in the
              rest. Nothing is baked: change a guide next week, or a slider the night before delivery, and every strand
              follows.
            </p>
          </Reveal>
          <Reveal delay={120}>
            <ol className="grid gap-8 sm:gap-0">
              {steps.map((s, i) => (
                <li
                  key={s.title}
                  className={cn('grid grid-cols-[auto_minmax(0,1fr)] gap-x-5 sm:py-6', i > 0 && 'sm:border-t sm:border-white/[0.09]')}
                >
                  <span aria-hidden="true" className="block size-10 text-gl-blue">
                    {s.icon}
                  </span>
                  <div>
                    <h3 className="flex items-baseline gap-3 text-[21px] font-semibold tracking-tight text-gl-ink">
                      <span className="font-mono text-[12px] font-normal text-gl-ink-3">0{i + 1}</span>
                      {s.title}
                    </h3>
                    <p className="mt-2 text-[15px] leading-relaxed text-gl-ink-2">{s.text}</p>
                  </div>
                </li>
              ))}
            </ol>
          </Reveal>
        </div>
      </div>
    </section>
  );
}

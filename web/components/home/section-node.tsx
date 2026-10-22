// SPDX-License-Identifier: Apache-2.0
import type { CSSProperties, ReactNode } from 'react';
import { Reveal } from '@/components/reveal';
import { cn } from '@/lib/cn';
import { category, type Category } from './graph-theme';

/*
 * Every section of the landing page is a node on one graph. Its tag is a node header with an input
 * port; on wide screens a short wire branches into that port from the page wire running down the
 * left margin (PageRail), drawing itself when the section comes into view.
 */

export function SectionTag({ cat, index, children }: { cat: Category; index: string; children: ReactNode }) {
  const c = category[cat];
  return (
    <span className="relative inline-flex h-8 items-center gap-2 rounded-[9px] border border-[#d9d9df] bg-white pr-3 pl-1 text-[13px] font-semibold shadow-[0_1px_0_rgba(0,0,0,0.03)]">
      {/* The branch from the page wire into the input port (wide screens only). */}
      <svg
        aria-hidden="true"
        viewBox="0 0 64 72"
        fill="none"
        className="pointer-events-none absolute top-1/2 right-full hidden h-[72px] w-16 -translate-y-[calc(100%-4px)] xl:block"
      >
        <path
          d="M1 0 C1 44, 18 68, 64 68"
          pathLength={1}
          className="strand draw-reveal"
          stroke={`url(#rail-branch-${cat})`}
          strokeWidth="2"
          style={{ '--d': '0.15s', '--dur': '0.9s' } as CSSProperties}
        />
        <defs>
          <linearGradient id={`rail-branch-${cat}`} x1="0" y1="0" x2="64" y2="68" gradientUnits="userSpaceOnUse">
            <stop offset="0" stopColor="#d4d4da" />
            <stop offset="1" stopColor={c.hue} />
          </linearGradient>
        </defs>
      </svg>
      <span aria-hidden="true" className="absolute top-1/2 -left-[5px] size-[9px] -translate-y-1/2 rounded-full border-2 border-white" style={{ background: c.hue }} />
      <span
        aria-hidden="true"
        className="grid h-6 min-w-6 place-items-center rounded-[6px] px-1 font-mono text-[11px] font-medium"
        style={{ background: c.tint, color: cat === 'Output' ? '#fff' : c.ink }}
      >
        {index}
      </span>
      <span>{children}</span>
      <span aria-hidden="true" className="absolute top-1/2 -right-[5px] size-[9px] -translate-y-1/2 rounded-full border-2 border-white bg-[#c9c9d0]" />
    </span>
  );
}

/** A section's opening: its tag and title on the left, the lede beside it on wide screens. */
export function SectionHead({
  id,
  cat,
  index,
  tag,
  title,
  muted,
  lede,
  className,
}: {
  id: string;
  cat: Category;
  index: string;
  tag: string;
  title: ReactNode;
  muted?: ReactNode;
  lede?: ReactNode;
  className?: string;
}) {
  return (
    <Reveal className={cn('grid gap-6 lg:grid-cols-12 lg:items-end lg:gap-12', className)}>
      <div className="lg:col-span-7">
        <SectionTag cat={cat} index={index}>
          {tag}
        </SectionTag>
        <h2 id={id} className="display t-section mt-6">
          {title}
          {muted && <span className="text-gl-ink-3"> {muted}</span>}
        </h2>
      </div>
      {lede && <div className="t-lede text-gl-ink-2 lg:col-span-5 lg:pb-2">{lede}</div>}
    </Reveal>
  );
}

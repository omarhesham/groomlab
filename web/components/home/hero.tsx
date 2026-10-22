// SPDX-License-Identifier: Apache-2.0
import Link from 'next/link';
import type { CSSProperties } from 'react';
import { pill } from '@/components/store-buttons';
import { TextLink } from '@/components/text-link';
import { cn } from '@/lib/cn';
import { siteConfig } from '@/site.config';
import { HeroGraph } from './hero-graph';

const delay = (s: number) => ({ '--d': `${s}s` }) as CSSProperties;

/** The opening: the promise in words, and a live node graph growing a groom beneath it. */
export function Hero() {
  return (
    <section id="hero" aria-labelledby="hero-title" className="relative pt-12 pb-20 sm:pt-16 sm:pb-24">
      <div className="wrap text-center">
        {/* Early access: honest about the stage, and where the changes are listed. */}
        <Link
          href="/docs/whats-new"
          className="rise inline-flex items-center gap-2 rounded-full border border-[#d9d9df] bg-white py-1 pr-3 pl-2.5 text-[12px] font-semibold text-gl-ink transition-colors hover:border-[#bdbdc5] focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-gl-blue sm:text-[13px]"
          style={delay(0)}
        >
          <span aria-hidden="true" className="size-1.5 rounded-full bg-gl-emerald" />
          Early access {siteConfig.version}
          <span aria-hidden="true" className="text-gl-ink-3">·</span>
          <span className="text-gl-ink-2">What&apos;s new</span>
        </Link>
        <p className="rise mt-5 font-mono text-[12px] font-medium tracking-[0.08em] text-gl-ink-2 uppercase sm:text-[13px]" style={delay(0.04)}>
          The Universal Groom System
        </p>
        <h1 id="hero-title" className="rise display t-hero mx-auto mt-3 max-w-[14ch] sm:max-w-none" style={delay(0.08)}>
          Groom once. <span className="hero-ink">Use it everywhere.</span>
        </h1>
        <p className="rise t-lede mx-auto mt-5 max-w-[38rem] text-gl-ink-2" style={delay(0.14)}>
          Hair, fur, feathers and scales, groomed with brushes and a node graph in Maya, Blender or the
          GroomLab app, and the same in Arnold, V-Ray, RenderMan, Unreal and Unity.
        </p>
        <div className="rise mt-8 flex flex-col items-center justify-center gap-4 sm:flex-row sm:gap-8" style={delay(0.2)}>
          <Link href="/download" className={cn(pill.base, pill.lg, pill.primary)}>
            Get GroomLab
          </Link>
          <TextLink href="#try">Try the node graph</TextLink>
        </div>
        <p className="rise mt-4 text-[13px] text-gl-ink-2" style={delay(0.24)}>
          Free and open source · Apache-2.0
        </p>
      </div>

      {/* The graph sits on its own patch of canvas, framed like a Graph Editor panel. */}
      <div className="rise wrap-wide mt-12 sm:mt-14" style={delay(0.3)}>
        <div className="relative">
          <div className="flex items-center justify-between px-3 pt-1 pb-2 font-mono text-[11px] text-gl-ink-3 sm:px-4 sm:text-[12px]">
            <span>
              <span className="text-gl-ink-2">head.groom</span> / graph
            </span>
            <span className="flex items-center gap-1.5">
              <span aria-hidden="true" className="live-dot size-1.5 rounded-full bg-gl-emerald" />
              live
            </span>
          </div>
          <HeroGraph />
        </div>
        <p className="sr-only">
          A node graph: Scalp and Guides feed Scatter and Interpolate, then Clump, Curl and HairColor shape and color
          the strands, and the Output node shows the groom growing.
        </p>
      </div>
    </section>
  );
}

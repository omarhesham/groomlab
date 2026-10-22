// SPDX-License-Identifier: Apache-2.0
import Link from 'next/link';
import type { CSSProperties } from 'react';
import { pill } from '@/components/store-buttons';
import { TextLink } from '@/components/text-link';
import { cn } from '@/lib/cn';
import { siteConfig } from '@/site.config';
import { HeroGraph } from './hero-graph';

const delay = (s: number) => ({ '--d': `${s}s` }) as CSSProperties;

/** The opening: the promise in words, the groom in 3D beneath it, and the graph that grows it. */
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

      {/* The groom, big and in 3D, and under it the graph that grows it. */}
      <div className="rise wrap-wide mt-9 sm:mt-11" style={delay(0.3)}>
        <HeroGraph />
        <p className="sr-only">
          A head of hair in 3D, turning slowly. Below it a node graph: Scalp and Guides feed Scatter and Interpolate,
          then Clump, Curl and HairColor shape and color the strands, and the last wire runs into the viewport.
        </p>
      </div>
    </section>
  );
}

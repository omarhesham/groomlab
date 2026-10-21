// SPDX-License-Identifier: Apache-2.0
import Image from 'next/image';
import Link from 'next/link';
import type { CSSProperties } from 'react';
import { pill } from '@/components/store-buttons';
import { TextLink } from '@/components/text-link';
import { cn } from '@/lib/cn';
import { siteConfig } from '@/site.config';
import { WorksWith } from './works-with';

const delay = (s: number) => ({ '--d': `${s}s` }) as CSSProperties;

// The artwork fills the screen behind the text. A light frosted band softens its bottom edge (a
// backdrop blur masked to the band) with a faint glow along it.
// A render of the wolf sample's fur, made with GroomLab.
const artwork = '/examples/hero.png';
const edgeBand = 'linear-gradient(to bottom, transparent 78%, #000 100%)';
// A soft blur over the whole artwork, under the text (the frosted band at the bottom is its own).
const artworkBlur = 'blur(16px)';

export function Hero() {
  return (
    <>
      <section
        id="hero"
        aria-labelledby="hero-title"
        className="relative -mt-12 flex min-h-[100svh] flex-col overflow-hidden bg-[#6b86ad] text-white"
      >
        <div aria-hidden="true" className="absolute inset-0">
          <Image src={artwork} alt="" fill sizes="100vw" priority className="object-cover object-[center_bottom]" />
          <div
            className="absolute inset-0"
            style={{ backdropFilter: artworkBlur, WebkitBackdropFilter: artworkBlur }}
          />
          <div
            className="absolute inset-0"
            style={{
              backdropFilter: 'blur(9px)',
              WebkitBackdropFilter: 'blur(9px)',
              maskImage: edgeBand,
              WebkitMaskImage: edgeBand,
            }}
          />
          <div
            className="absolute inset-0"
            style={{
              boxShadow: 'inset 0 -140px 120px -100px rgba(225, 235, 250, 0.18)',
            }}
          />
          {/* A little shade under the header and the headline, for the white text. */}
          <div className="absolute inset-x-0 top-0 h-[35%] bg-gradient-to-b from-black/15 to-transparent" />
          {/* On phones the buttons sit at the bottom of the art: a little shade there too. */}
          <div className="absolute inset-x-0 bottom-0 h-[30%] bg-gradient-to-t from-black/20 to-transparent sm:hidden" />
        </div>

        {/* Short copy, so the art carries the hero. */}
        <div className="wrap relative pt-20 text-center sm:pt-[max(5rem,9svh)] lg:pt-[max(6rem,15svh)] [@media(min-width:1024px)_and_(min-height:1000px)]:pt-[21svh]">
          {/* Early access: honest about the stage, and where the changes are listed. */}
          <Link
            href="/docs/whats-new"
            className="rise mb-4 inline-flex items-center gap-2 rounded-full bg-white/15 py-1 pr-3 pl-2.5 text-[12px] font-semibold text-white ring-1 ring-white/30 backdrop-blur-sm transition-colors hover:bg-white/25 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-white sm:mb-5 sm:text-[13px]"
            style={delay(0)}
          >
            <span aria-hidden="true" className="size-1.5 rounded-full bg-gl-emerald" />
            Early access {siteConfig.version}
            <span aria-hidden="true" className="text-white/50">·</span>
            <span className="text-white/85">What&apos;s new</span>
          </Link>
          <p className="rise text-[15px] font-semibold text-white/85 drop-shadow-sm sm:text-[17px]" style={delay(0)}>
            The Universal Groom System
          </p>
          <h1
            id="hero-title"
            className="rise display t-hero mt-3 text-[clamp(2.25rem,min(4.5vw,7.5svh),4.25rem)] drop-shadow-[0_2px_20px_rgba(20,30,50,0.25)]"
            style={delay(0.08)}
          >
            Groom once. Use it everywhere.
          </h1>
          <p
            className="rise mx-auto mt-4 max-w-[30rem] text-[15px] leading-snug text-white/90 drop-shadow-sm sm:mt-5 sm:max-w-[36rem] sm:text-[19px]"
            style={delay(0.14)}
          >
            Hair, fur, feathers and scales, groomed in Maya or the GroomLab app and identical in Arnold, Unreal and
            Unity.
          </p>
        </div>
        {/* Under the headline on wider screens; at the bottom of the art on phones, clear of the wolf. */}
        <div className="wrap relative mt-auto pb-[max(3.5rem,env(safe-area-inset-bottom))] sm:mt-7 sm:pb-0">
          <div
            className="rise flex flex-col items-center justify-center gap-4 sm:flex-row sm:gap-8"
            style={delay(0.2)}
          >
            <Link
              href="/download"
              className={cn(
                pill.base,
                pill.lg,
                'bg-white text-[#5D779C] hover:bg-white/90',
              )}
              style={{ boxShadow: '0 12px 28px -10px rgba(255, 255, 255, 0.7)' }}
            >
              Get GroomLab
            </Link>
            <TextLink href="#learn-more" className="text-white focus-visible:outline-white">
              Learn more
            </TextLink>
          </div>
        </div>
      </section>

      <div id="learn-more" className="wrap-wide scroll-mt-16 pb-20 sm:pb-28">
        <WorksWith />
      </div>
    </>
  );
}

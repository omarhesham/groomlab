// SPDX-License-Identifier: Apache-2.0
import { EveryApp } from '@/components/home/every-app';
import { Hero } from '@/components/home/hero';
import { Inspector } from '@/components/home/inspector';
import { OpenSource } from '@/components/home/open-source';
import { OutputCta } from '@/components/home/output-cta';
import { PageRail } from '@/components/home/page-rail';
import { PlaySection } from '@/components/home/play-section';
import { Toolkit } from '@/components/home/toolkit';
import { TrySection } from '@/components/home/try-section';
import { UnderTheHood } from '@/components/home/under-the-hood';

// The hero: a head of curls in 3D beside the promise. Then how a groom is made, in words, and
// below it the page is one graph, every section a node on the page wire: try the graph, one groom
// in every app, the toolkit, the proof (performance), the depth (under the hood), open source, then
// getting it.
export default function HomePage() {
  return (
    <>
      <Hero />
      <PlaySection />
      <div className="relative">
        <PageRail />
        <TrySection />
        <EveryApp />
        <Toolkit />
        <Inspector />
        <UnderTheHood />
        <OpenSource />
      </div>
      <OutputCta />
    </>
  );
}

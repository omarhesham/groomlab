// SPDX-License-Identifier: Apache-2.0
import { CtaBand } from '@/components/home/cta-band';
import { Features } from '@/components/home/features';
import { Hero } from '@/components/home/hero';
import { Hosts } from '@/components/home/hosts';
import { Performance } from '@/components/home/performance';
import { Renderers } from '@/components/home/renderers';
import { UnderTheHood } from '@/components/home/under-the-hood';

// The story: what it is (hero), the promise (one groom, every app), where it renders, what grooming in it is like
// (features, game cards), the proof (performance), the depth (under the hood), then getting it.
export default function HomePage() {
  return (
    <>
      <Hero />
      <Hosts />
      <Renderers />
      <Features />
      <Performance />
      <UnderTheHood />
      <CtaBand />
    </>
  );
}

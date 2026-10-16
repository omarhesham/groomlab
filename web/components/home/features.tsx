// SPDX-License-Identifier: Apache-2.0
import { ExampleImage } from '@/components/example-image';
import { Reveal } from '@/components/reveal';
import { TextLink } from '@/components/text-link';
import { FeatureSection } from './feature-section';
import { StrandFlow } from './strands';

export function Features() {
  return (
    <>
      <section id="features" aria-labelledby="features-title" className="section-y">
        <div className="wrap">
          <Reveal className="mx-auto max-w-3xl text-center">
            <h2 id="features-title" className="display t-section">
              Everything a groom needs. <span className="text-gl-ink-3">Nothing it doesn&apos;t.</span>
            </h2>
          </Reveal>

          <div className="mt-20 space-y-28 sm:mt-28 sm:space-y-40">
            <FeatureSection
              eyebrow="Groom by hand"
              title="Brush the guides. The hair follows."
              accent="blue"
              href="/docs/guides-and-brushes"
              media={<GroomByHandMedia />}
            >
              <p>Comb the guides with brushes. The hair follows, live.</p>
            </FeatureSection>

            <FeatureSection
              eyebrow="Node graph"
              title="Style with a node graph."
              accent="yellow"
              href="/docs/node-graph"
              reverse
              media={
                <ExampleImage
                  name="graph-editor.png"
                  alt="The GroomLab Graph Editor with a look built from nodes"
                  caption="The Graph Editor"
                  sizes="(min-width: 1080px) 620px, 100vw"
                  className="shadow-[0_30px_60px_-30px_rgba(28,29,33,0.45)]"
                  parallax
                />
              }
            >
              <p>Clumps, curls, frizz and braids are nodes. Sliders update 100k strands live.</p>
            </FeatureSection>

            <FeatureSection
              eyebrow="Paint anything"
              title="Paint any parameter."
              accent="emerald"
              href="/docs/painting-maps"
              media={
                <ExampleImage
                  name="paint-maps.png"
                  alt="Painting a map on the scalp in GroomLab's Paint tab"
                  caption="Painted maps"
                  sizes="(min-width: 1080px) 620px, 100vw"
                  className="shadow-[0_30px_60px_-30px_rgba(28,29,33,0.45)]"
                  parallax
                />
              }
            >
              <p>Right-click any parameter and paint it on the scalp.</p>
            </FeatureSection>
          </div>
        </div>
      </section>

      <GameCards />
    </>
  );
}

/** The dark beat of the page: groom to game engine, the lock of strands drawn behind the copy. */
function GameCards() {
  return (
    <section
      aria-labelledby="cards-title"
      className="relative overflow-clip bg-gl-night py-28 text-gl-white [color-scheme:dark] sm:py-40"
    >
      <Reveal className="absolute inset-x-0 top-1/2 -translate-y-1/2">
        <StrandFlow range="entry 0% cover 40%" />
      </Reveal>
      <div className="wrap relative">
        <Reveal className="mx-auto max-w-3xl text-center">
          <p className="t-eyebrow text-gl-red">Game-ready cards</p>
          <h2 id="cards-title" className="display t-section mt-3">
            From groom to game engine.
          </h2>
          <p className="t-lede mx-auto mt-6 max-w-2xl text-gl-dim">
            Cards baked from your own strands, ready for Unreal, Unity and glTF.
          </p>
          <TextLink href="/docs/game-export" tone="dark" className="mt-7">
            Learn more<span className="sr-only"> about game export</span>
          </TextLink>
        </Reveal>
      </div>
    </section>
  );
}

function GroomByHandMedia() {
  return (
    <div className="relative pb-12 sm:pr-16">
      <ExampleImage
        name="viewport-guides.png"
        alt="Guides drawn over a groom in Maya's viewport, colored by hierarchy level"
        caption="Guides in the viewport"
        sizes="(min-width: 1080px) 560px, 100vw"
        className="shadow-[0_30px_60px_-30px_rgba(28,29,33,0.45)]"
        parallax
      />
      <div className="float-scroll absolute right-0 bottom-0 w-[36%] max-w-[220px]">
        <div className="animate-bob">
          <ExampleImage
            name="groom-tools.png"
            alt="The Groom Tools panel with its brushes"
            caption="Groom Tools"
            aspect="3 / 4"
            position="top"
            sizes="220px"
            className="rounded-[16px] shadow-[0_30px_60px_-20px_rgba(0,0,0,0.45)] ring-4 ring-white sm:rounded-[20px]"
          />
        </div>
      </div>
    </div>
  );
}

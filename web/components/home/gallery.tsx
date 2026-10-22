// SPDX-License-Identifier: Apache-2.0
import Image from 'next/image';
import { Reveal } from '@/components/reveal';
import { getExample } from '@/lib/examples';
import { category, type Category } from './graph-theme';
import { SectionHead } from './section-node';

/* The page's only pictures: real captures from the tools, each in a viewport panel. */
const shots: { name: string; title: string; alt: string; cat: Category }[] = [
  { name: 'graph-editor.png', title: 'Graph Editor', alt: 'The GroomLab Graph Editor with a look built from nodes', cat: 'Utility' },
  { name: 'viewport-guides.png', title: 'Guides', alt: 'Guides drawn over a groom in the viewport, colored by hierarchy level', cat: 'Shape' },
  { name: 'game-cards.png', title: 'Cards', alt: 'A wolf groom as strands, as cards with baked maps, and as card geometry', cat: 'Games' },
  { name: 'feathers.png', title: 'Feathers', alt: 'A round blue bird covered in procedural feathers, with yellow wing and tail feathers', cat: 'Feathers' },
];

export function Gallery() {
  const available = shots.flatMap((s) => {
    const info = getExample(s.name);
    return info ? [{ ...s, info }] : [];
  });
  if (!available.length) return null;
  return (
    <section aria-labelledby="gallery-title" className="section-y">
      <div className="wrap">
        <SectionHead
          id="gallery-title"
          cat="Grow"
          index="06"
          tag="In the tools"
          title="See it in the tools."
          lede={<p>Captured from GroomLab as it runs: the graph, the guides, game cards and feathers.</p>}
        />
      </div>
      <Reveal className="mt-12 sm:mt-14">
        <ul className="no-scrollbar wrap-wide flex snap-x snap-mandatory gap-4 overflow-x-auto pb-2 md:grid md:grid-cols-2 md:gap-5 md:overflow-visible">
          {available.map((s) => {
            const c = category[s.cat];
            return (
              <li key={s.name} className="w-[86%] shrink-0 snap-center md:w-auto">
                <figure className="overflow-hidden rounded-[16px]">
                  <figcaption className="flex h-10 items-center gap-2.5 border-b border-[#e6e6eb] px-3.5 text-[13px] font-semibold">
                    <span aria-hidden="true" className="size-2 rounded-full" style={{ background: c.hue }} />
                    {s.title}
                    <span className="ml-auto font-mono text-[11px] font-normal text-gl-ink-3">{s.name}</span>
                  </figcaption>
                  <div className="relative aspect-[16/10] bg-gl-night">
                    <Image
                      src={s.info.src}
                      alt={s.alt}
                      fill
                      sizes="(min-width: 1240px) 610px, (min-width: 768px) 50vw, 86vw"
                      className="object-cover transition-transform duration-700 ease-out-soft hover:scale-[1.02]"
                    />
                  </div>
                </figure>
              </li>
            );
          })}
        </ul>
      </Reveal>
    </section>
  );
}

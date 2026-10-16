// SPDX-License-Identifier: Apache-2.0
import { Reveal } from '@/components/reveal';
import { accentColor, type Accent } from './feature-section';

type Destination = { name: string; accent: Accent };

const left: Destination[] = [
  { name: 'Maya', accent: 'blue' },
  { name: 'Blender', accent: 'yellow' },
  { name: 'GroomLab app', accent: 'red' },
];
const right: Destination[] = [
  { name: 'Render farm', accent: 'emerald' },
  { name: 'Unreal', accent: 'purple' },
  { name: 'Unity', accent: 'blue' },
];

const tags = ['Identical everywhere', 'Tiny files', 'Always editable'];

// Three 46px pills 22px apart: their middles, and the diagram's height.
const PILL_MIDDLES = [23, 91, 159];
const HEIGHT = 182;

/** Strands from the file's side to each pill's middle (`mirror`: the left side, curving the other way). */
function Strands({ items, mirror }: { items: Destination[]; mirror?: boolean }) {
  const mid = HEIGHT / 2;
  return (
    <svg
      aria-hidden="true"
      viewBox={`0 0 120 ${HEIGHT}`}
      preserveAspectRatio="none"
      className="hidden h-[182px] w-24 shrink-0 md:block lg:w-32"
      fill="none"
    >
      {items.map((d, i) => {
        const y = PILL_MIDDLES[i];
        const path = mirror ? `M120 ${mid} C 60 ${mid}, 60 ${y}, 0 ${y}` : `M0 ${mid} C 60 ${mid}, 60 ${y}, 120 ${y}`;
        return (
          <path key={d.name} d={path} stroke={accentColor[d.accent]} strokeWidth="3" strokeLinecap="round" vectorEffect="non-scaling-stroke" />
        );
      })}
    </svg>
  );
}

function Pills({ items, align }: { items: Destination[]; align: 'start' | 'end' }) {
  return (
    <ul className={`flex flex-col gap-[22px] ${align === 'end' ? 'md:items-end' : 'md:items-start'} items-center`}>
      {items.map((d) => (
        <li
          key={d.name}
          className="flex h-[46px] items-center gap-2.5 rounded-full border border-black/[0.06] bg-white px-5 text-[15px] font-medium"
        >
          <span aria-hidden="true" className="size-2.5 rounded-full" style={{ background: accentColor[d.accent] }} />
          {d.name}
        </li>
      ))}
    </ul>
  );
}

/** The groom file, the hair's colors coming out of it. */
function FileCard() {
  return (
    <div className="flex w-44 shrink-0 flex-col items-center rounded-[22px] border border-black/[0.06] bg-white p-6 text-center">
      <svg aria-hidden="true" viewBox="0 0 40 48" className="h-14 w-12" fill="none">
        <path d="M6 3h20l10 10v30a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2Z" fill="#F4F3EF" stroke="#1C1D21" strokeWidth="2" />
        <path d="M26 3v10h10" stroke="#1C1D21" strokeWidth="2" strokeLinejoin="round" />
        <path d="M12 38c0-7 2-12 7-14" stroke="#FF4D4F" strokeWidth="2.6" strokeLinecap="round" />
        <path d="M17 38c0-6 2-10 6-12" stroke="#FFC53D" strokeWidth="2.6" strokeLinecap="round" />
        <path d="M22 38c0-5 2-8 6-9" stroke="#2F7BF6" strokeWidth="2.6" strokeLinecap="round" />
      </svg>
      <p className="mt-3 font-mono text-[15px] font-semibold">wolf.groom</p>
    </div>
  );
}

export function Hosts() {
  return (
    <section aria-labelledby="hosts-title" className="section-y">
      <div className="wrap flex flex-col items-center text-center">
        <Reveal>
          <h2 id="hosts-title" className="display t-section">
            One groom. Every app. <span className="text-gl-ink-3">The same hair.</span>
          </h2>
        </Reveal>

        <Reveal className="mt-12 flex flex-col items-center gap-8 sm:mt-14 md:flex-row md:gap-0">
          <div className="order-2 md:order-1 md:flex md:items-center">
            <Pills items={left} align="end" />
            <Strands items={left} mirror />
          </div>
          <div className="order-1 md:order-2">
            <FileCard />
          </div>
          <div className="order-3 md:flex md:items-center">
            <Strands items={right} />
            <Pills items={right} align="start" />
          </div>
        </Reveal>

        <Reveal>
          <ul className="mt-12 flex flex-wrap justify-center gap-x-8 gap-y-2 text-[15px] font-medium text-gl-ink-2 sm:mt-14">
            {tags.map((t) => (
              <li key={t}>{t}</li>
            ))}
          </ul>
        </Reveal>
      </div>
    </section>
  );
}

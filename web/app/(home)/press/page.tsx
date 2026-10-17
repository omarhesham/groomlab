// SPDX-License-Identifier: Apache-2.0
import { statSync } from 'node:fs';
import { join } from 'node:path';
import type { Metadata } from 'next';
import type { ReactNode } from 'react';
import { ArrowDownToLine, Check, X } from 'lucide-react';
import { pill } from '@/components/store-buttons';
import { cn } from '@/lib/cn';
import { siteConfig } from '@/site.config';

export const metadata: Metadata = {
  title: 'Press kit',
  description: 'The GroomLab logo in SVG and PNG, the brand colors, and how to write the name and use the logo.',
};

/** The files are made by tools/web/make_press_kit.py into public/press (names: groomlab-<piece>-<version>). */
const PRESS = '/press';
const ZIP = `${PRESS}/groomlab-press-kit.zip`;

type Bg = 'light' | 'dark';
interface Asset {
  version: string;
  label: string;
  /** The backgrounds it's shown on (the color mark works on both). */
  on: Bg[];
}
interface Piece {
  id: string;
  title: string;
  what: string;
  /** Preview height on its card. */
  size: string;
  assets: Asset[];
}

const pieces: Piece[] = [
  {
    id: 'lockup',
    title: 'Logo',
    what: 'The mark and the name together: the main logo, for most uses.',
    size: 'h-10 sm:h-12',
    assets: [
      { version: 'color', label: 'Color', on: ['light'] },
      { version: 'color-on-dark', label: 'Color on dark', on: ['dark'] },
      { version: 'black', label: 'Black', on: ['light'] },
      { version: 'white', label: 'White', on: ['dark'] },
    ],
  },
  {
    id: 'mark',
    title: 'Mark',
    what: 'Three strands rising from the scalp. For icons, avatars and small spaces where the name is nearby.',
    size: 'h-20',
    assets: [
      { version: 'color', label: 'Color', on: ['light', 'dark'] },
      { version: 'black', label: 'Black', on: ['light'] },
      { version: 'white', label: 'White', on: ['dark'] },
    ],
  },
  {
    id: 'wordmark',
    title: 'Wordmark',
    what: 'The name alone, set in Inter Bold as on this site. The white version is the one for dark backgrounds.',
    size: 'h-9 sm:h-10',
    assets: [
      { version: 'color', label: 'Ink', on: ['light'] },
      { version: 'black', label: 'Black', on: ['light'] },
      { version: 'white', label: 'White', on: ['dark'] },
    ],
  },
];

const colors = [
  { name: 'Red', hex: '#FF4D4F', use: 'First strand' },
  { name: 'Yellow', hex: '#FFC53D', use: 'Second strand' },
  { name: 'Blue', hex: '#2F7BF6', use: 'Third strand, links' },
  { name: 'Ink', hex: '#1D1D1F', use: 'The name on light backgrounds' },
  { name: 'Night', hex: '#1C1D21', use: 'Dark backgrounds' },
  { name: 'Paper', hex: '#FFFFFF', use: 'Light backgrounds' },
];

const nameDont = ['Groom Lab', 'Groomlab', 'groomLab', 'GROOMLAB', 'Groom-Lab', 'GL'];

const zipSize = () => {
  try {
    return `${(statSync(join(process.cwd(), 'public', 'press', 'groomlab-press-kit.zip')).size / 1e6).toFixed(1)} MB`;
  } catch {
    return null;
  }
};

const preview: Record<Bg, string> = {
  light: 'bg-white ring-1 ring-inset ring-black/[0.06]',
  dark: 'bg-gl-night',
};

function Section({ id, title, intro, children }: { id: string; title: string; intro?: ReactNode; children: ReactNode }) {
  return (
    <section id={id} aria-labelledby={`${id}-title`} className="mt-16 sm:mt-20">
      <h2 id={`${id}-title`} className="text-[1.75rem] font-semibold tracking-tight">
        {title}
      </h2>
      {intro && <div className="mt-2 max-w-2xl text-[15px] leading-relaxed text-gl-ink-2 text-pretty">{intro}</div>}
      <div className="mt-6">{children}</div>
    </section>
  );
}

function Downloads({ stem, label }: { stem: string; label: string }) {
  const files = [
    { href: `${PRESS}/${stem}.svg`, text: 'SVG' },
    { href: `${PRESS}/${stem}-512.png`, text: 'PNG 512' },
    { href: `${PRESS}/${stem}-2048.png`, text: 'PNG 2048' },
  ];
  return (
    <ul className="flex flex-wrap gap-2" aria-label={`Download ${label}`}>
      {files.map((f) => (
        <li key={f.text}>
          <a
            href={f.href}
            download
            className="inline-flex h-8 items-center gap-1.5 rounded-full bg-white px-3 text-[13px] font-medium text-gl-blue-strong ring-1 ring-black/[0.08] transition hover:ring-black/[0.16] focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-gl-blue"
          >
            <ArrowDownToLine aria-hidden="true" className="size-3.5" strokeWidth={2.4} />
            {f.text}
          </a>
        </li>
      ))}
    </ul>
  );
}

export default function PressPage() {
  const size = zipSize();
  return (
    <article aria-labelledby="press-title" className="wrap py-20 sm:py-28">
      <header className="mx-auto max-w-2xl text-center">
        <h1 id="press-title" className="display t-section">
          Press kit
        </h1>
        <p className="mt-5 text-[17px] leading-snug text-gl-ink-2 text-pretty">
          The GroomLab logo for articles, videos, tutorials and tools that work with GroomLab, with the colors and a few
          rules for using them.
        </p>
        <div className="mt-8 flex flex-col items-center gap-2">
          <a href={ZIP} download className={cn(pill.base, pill.md, pill.primary)}>
            Download everything (.zip)
            <ArrowDownToLine aria-hidden="true" className="size-4 opacity-80" strokeWidth={2.4} />
          </a>
          <p className="text-[13px] text-gl-ink-2">
            Every logo as SVG and PNG, with a README{size && ` · ${size}`}
          </p>
        </div>
      </header>

      <div className="mx-auto max-w-4xl">
        <Section
          id="name"
          title="Writing the name"
          intro={
            <p>
              Write it <strong className="font-semibold text-gl-ink">GroomLab</strong>: one word, capital G and capital L,
              in running text, headlines and titles.
            </p>
          }
        >
          <div className="grid gap-4 sm:grid-cols-2">
            <div className="rounded-[28px] bg-gl-mist p-6">
              <p className="flex items-center gap-2 text-[13px] font-semibold tracking-wide text-gl-ink-2 uppercase">
                <Check aria-hidden="true" className="size-4 text-gl-emerald" strokeWidth={3} />
                Do
              </p>
              <ul className="mt-3 space-y-2 text-[15px] leading-relaxed">
                <li>
                  <span className="text-xl font-bold tracking-tight">GroomLab</span>
                </li>
                <li>
                  Possessive: <span className="font-semibold">GroomLab&apos;s</span>
                </li>
                <li>
                  Products: <span className="font-semibold">GroomLab for Maya</span>,{' '}
                  <span className="font-semibold">GroomLab for Blender</span>,{' '}
                  <span className="font-semibold">the GroomLab app</span>
                </li>
              </ul>
            </div>
            <div className="rounded-[28px] bg-gl-mist p-6">
              <p className="flex items-center gap-2 text-[13px] font-semibold tracking-wide text-gl-ink-2 uppercase">
                <X aria-hidden="true" className="size-4 text-gl-red" strokeWidth={3} />
                Don&apos;t
              </p>
              <ul className="mt-3 flex flex-wrap gap-2" aria-label="Spellings not to use">
                {nameDont.map((n) => (
                  <li
                    key={n}
                    className="rounded-full bg-white px-3 py-1 text-[15px] text-gl-ink-2 line-through decoration-gl-red/70 ring-1 ring-black/[0.06]"
                  >
                    {n}
                  </li>
                ))}
              </ul>
              <p className="mt-4 text-[14px] leading-relaxed text-gl-ink-2">
                Don&apos;t split, hyphenate, recase or abbreviate it. Lowercase{' '}
                <code className="font-mono text-[13px] text-gl-ink">groomlab</code> only where a system needs it: URLs,
                package and file names, handles (getgroomlab.com, github.com/thekesslereffect/groomlab).
              </p>
            </div>
          </div>
        </Section>

        {pieces.map((piece) => (
          <Section key={piece.id} id={piece.id} title={piece.title} intro={<p>{piece.what}</p>}>
            <ul className="grid gap-4 sm:grid-cols-2">
              {piece.assets.map((a) => {
                const stem = `groomlab-${piece.id}-${a.version}`;
                const label = `${piece.title}, ${a.label.toLowerCase()}`;
                return (
                  <li key={a.version} className="rounded-[28px] bg-gl-mist p-3">
                    <div className={cn('grid overflow-hidden rounded-[20px]', a.on.length > 1 && 'grid-cols-2')}>
                      {a.on.map((bg) => (
                        <div key={bg} className={cn('flex h-44 items-center justify-center px-6', preview[bg])}>
                          {/* eslint-disable-next-line @next/next/no-img-element -- static SVG, shown as it is */}
                          <img src={`${PRESS}/${stem}.svg`} alt={`GroomLab ${label}`} className={cn('w-auto max-w-full', piece.size)} />
                        </div>
                      ))}
                    </div>
                    <div className="flex flex-wrap items-center justify-between gap-3 px-3 pt-4 pb-2">
                      <p className="text-[15px] font-semibold">{a.label}</p>
                      <Downloads stem={stem} label={label} />
                    </div>
                  </li>
                );
              })}
            </ul>
          </Section>
        ))}

        <Section id="colors" title="Colors" intro={<p>The strands&apos; primaries and the site&apos;s ink and night.</p>}>
          <ul className="grid grid-cols-2 gap-4 sm:grid-cols-3">
            {colors.map((c) => (
              <li key={c.hex} className="overflow-hidden rounded-[20px] bg-gl-mist">
                <div className="h-20 ring-1 ring-inset ring-black/[0.06]" style={{ backgroundColor: c.hex }} />
                <div className="px-4 py-3">
                  <p className="text-[15px] font-semibold">{c.name}</p>
                  <p className="font-mono text-[13px] text-gl-ink">{c.hex}</p>
                  <p className="mt-0.5 text-[13px] text-gl-ink-2">{c.use}</p>
                </div>
              </li>
            ))}
          </ul>
        </Section>

        <Section id="usage" title="Using the logo">
          <ul className="space-y-3 text-[15px] leading-relaxed text-gl-ink-2 [&_li]:flex [&_li]:gap-3">
            {[
              <>
                Use the files as they are: no recoloring beyond the versions here, no stretching or squashing, no effects
                (shadows, outlines, gradients), and don&apos;t rearrange, crop or redraw the mark and the name.
              </>,
              <>Keep clear space around the logo: at least half the mark&apos;s height on every side.</>,
              <>
                Color versions on white or light backgrounds; the color-on-dark logo or the white versions on dark
                backgrounds and photos; black where only one dark color prints.
              </>,
              <>
                When you mention GroomLab, link to{' '}
                <a className="text-gl-blue-strong hover:underline" href={siteConfig.url}>
                  getgroomlab.com
                </a>
                .
              </>,
              <>
                Describe your work as &ldquo;works with GroomLab&rdquo; or &ldquo;Hair from GroomLab&rdquo;. Don&apos;t
                present it as official, as a partnership, or as endorsed by GroomLab.
              </>,
            ].map((rule, i) => (
              <li key={i}>
                <span aria-hidden="true" className="mt-[0.6em] size-1.5 shrink-0 rounded-full bg-gl-blue" />
                <span>{rule}</span>
              </li>
            ))}
          </ul>
        </Section>

        <Section id="trademarks" title="Trademarks">
          <p className="max-w-2xl text-[15px] leading-relaxed text-gl-ink-2 text-pretty">
            GroomLab&apos;s source code is open source under the Apache License 2.0, which doesn&apos;t grant rights to the
            GroomLab name or logo. GroomLab and the GroomLab logo are trademarks of Nick Rutlinh; the files on this page
            may be used to refer to GroomLab as described above. Questions, or a use not covered here? Write to{' '}
            <a className="text-gl-blue-strong hover:underline" href={`mailto:${siteConfig.supportEmail}`}>
              {siteConfig.supportEmail}
            </a>
            .
          </p>
        </Section>
      </div>
    </article>
  );
}

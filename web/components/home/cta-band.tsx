// SPDX-License-Identifier: Apache-2.0
import { Reveal } from '@/components/reveal';
import { StoreButtons } from '@/components/store-buttons';
import { TextLink } from '@/components/text-link';
import { siteConfig } from '@/site.config';
import { StrandFlow } from './strands';

const steps = [
  {
    title: 'Download',
    text: 'Free, every feature, any use.',
    ball: 'bg-gl-emerald text-gl-ink',
  },
  { title: 'Groom', text: 'Brush guides, build the look.', ball: 'bg-gl-blue text-white' },
  {
    title: 'Export',
    text: 'Render, or send it to your engine.',
    ball: 'bg-gl-yellow text-gl-ink',
  },
];

/** The close: how getting started goes, in three steps, and where to get it. */
export function CtaBand() {
  const req = siteConfig.requirements;
  return (
    <section id="get" aria-labelledby="get-title" className="pt-8 pb-24 sm:pb-32">
      <Reveal className="wrap-wide relative overflow-hidden rounded-[32px] bg-gl-mist sm:rounded-[44px]">
        <div className="relative z-10 px-6 pt-16 pb-8 text-center sm:pt-24">
          <h2 id="get-title" className="display t-section">
            Start grooming.
          </h2>
          <ol className="mx-auto mt-10 grid max-w-4xl gap-8 text-left sm:mt-12 sm:grid-cols-3 sm:gap-10">
            {steps.map(({ title, text, ball }, i) => (
              <li key={title} className="flex gap-4 sm:flex-col sm:items-center sm:text-center">
                <span
                  aria-hidden="true"
                  className={`grid size-10 shrink-0 place-items-center rounded-full text-lg font-bold shadow-[0_8px_16px_-8px_rgba(0,0,0,0.35)] ${ball}`}
                >
                  {i + 1}
                </span>
                <div>
                  <h3 className="text-lg font-semibold tracking-tight">{title}</h3>
                  <p className="mt-1 text-[15px] leading-snug text-gl-ink-2">{text}</p>
                </div>
              </li>
            ))}
          </ol>
          <StoreButtons className="mx-auto mt-12 max-w-xs sm:max-w-none" />
          <p className="mx-auto mt-6 max-w-xl text-sm text-gl-ink-2">
            Free and open source, for any use.{' '}
            <a className="text-gl-blue-strong hover:underline" href={siteConfig.repoUrl}>
              Source on GitHub
            </a>
            . If it helps you, support it on Ko-fi.
          </p>
          <p className="mt-2 text-sm text-gl-ink-2">
            {req.maya} on {req.os} <span aria-hidden="true">·</span> Early access:{' '}
            <a className="text-gl-blue-strong hover:underline" href={`mailto:${siteConfig.supportEmail}`}>
              tell us what you need
            </a>
          </p>
          <div className="mt-5 flex flex-col items-center justify-center gap-3 sm:flex-row sm:gap-8">
            <TextLink href={siteConfig.discordUrl}>Join the Discord</TextLink>
            <TextLink href="/docs/installation">Read the install guide</TextLink>
            <TextLink href="/docs/whats-new">What&apos;s new</TextLink>
          </div>
        </div>
        {/* Its own reveal, so the lock draws in full once it is on screen. */}
        <Reveal>
          <StrandFlow scroll={false} />
        </Reveal>
      </Reveal>
    </section>
  );
}

// SPDX-License-Identifier: Apache-2.0
import { Reveal } from '@/components/reveal';
import { StoreButtons } from '@/components/store-buttons';
import { TextLink } from '@/components/text-link';
import { siteConfig } from '@/site.config';
import { category } from './graph-theme';
import { SectionTag } from './section-node';

const steps = [
  { title: 'Download', text: 'Free, every feature, any use.', cat: 'Sources' as const },
  { title: 'Groom', text: 'Brush guides, build the look.', cat: 'Shape' as const },
  { title: 'Export', text: 'Render, or send it to your engine.', cat: 'Games' as const },
];

/** The close, as the graph's Output node: three steps in, GroomLab out. */
export function OutputCta() {
  const req = siteConfig.requirements;
  return (
    <section id="get" aria-labelledby="get-title" className="pt-8 pb-24 sm:pb-32">
      <div className="wrap">
        <Reveal>
          <SectionTag cat="Output" index="08">
            Output
          </SectionTag>
        </Reveal>
        <Reveal className="relative mt-6">
          <div className="relative overflow-hidden rounded-[24px] border border-[#1d1d1f] bg-white shadow-[0_50px_100px_-60px_rgba(28,29,33,0.55)] sm:rounded-[30px]">
            <div className="flex h-12 items-center gap-3 bg-gl-ink px-5 text-white sm:h-14 sm:px-7">
              <span aria-hidden="true" className="size-2.5 rounded-full bg-white" />
              <span className="text-[15px] font-semibold">Output</span>
              <span className="ml-auto font-mono text-[12px] text-white/60">groomlab {siteConfig.version}</span>
            </div>
            <div className="px-6 pt-14 pb-12 text-center sm:px-10 sm:pt-20 sm:pb-16">
              <h2 id="get-title" className="display t-section">
                Start grooming.
              </h2>
              <ol className="mx-auto mt-10 grid max-w-3xl gap-3 text-left sm:mt-12 sm:grid-cols-3">
                {steps.map(({ title, text, cat }, i) => {
                  const c = category[cat];
                  return (
                    <li key={title} className="relative rounded-[12px] border border-[#d9d9df] bg-white">
                      <span aria-hidden="true" className="absolute top-[19px] -left-[5px] size-[9px] rounded-full border-2 border-white" style={{ background: c.hue }} />
                      <div className="flex h-10 items-center gap-2.5 rounded-t-[11px] px-3.5" style={{ background: c.tint }}>
                        <span aria-hidden="true" className="font-mono text-[11px] font-medium" style={{ color: c.ink }}>
                          0{i + 1}
                        </span>
                        <h3 className="text-[15px] font-semibold tracking-tight">{title}</h3>
                      </div>
                      <p className="px-3.5 py-3 text-[14px] leading-snug text-gl-ink-2">{text}</p>
                    </li>
                  );
                })}
              </ol>
              <StoreButtons className="mx-auto mt-10 max-w-xs sm:max-w-none" />
              <p className="mx-auto mt-6 max-w-2xl text-sm text-gl-ink-2">
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
              <div className="mt-6 flex flex-col items-center justify-center gap-3 sm:flex-row sm:gap-8">
                <TextLink href={siteConfig.discordUrl}>Join the Discord</TextLink>
                <TextLink href="/docs/installation">Read the install guide</TextLink>
                <TextLink href="/docs/whats-new">What&apos;s new</TextLink>
              </div>
            </div>
          </div>
        </Reveal>
      </div>
    </section>
  );
}

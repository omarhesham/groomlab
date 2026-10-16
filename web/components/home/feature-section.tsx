// SPDX-License-Identifier: Apache-2.0
import type { ReactNode } from 'react';
import { Reveal } from '@/components/reveal';
import { TextLink } from '@/components/text-link';
import { cn } from '@/lib/cn';
import { Curl } from './strands';

export type Accent = 'blue' | 'yellow' | 'red' | 'emerald' | 'purple';

/** The primaries, deepened enough to read as text on paper. */
export const accentText: Record<Accent, string> = {
  blue: 'text-gl-blue-strong',
  yellow: 'text-[#E8A317]',
  red: 'text-[#d92d30]',
  emerald: 'text-[#0f8a5c]',
  purple: 'text-[#7a3ee8]',
};

/** The primaries as they are, for strands and dots. */
export const accentColor: Record<Accent, string> = {
  blue: '#2F7BF6',
  yellow: '#FFC53D',
  red: '#FF4D4F',
  emerald: '#2ECC8F',
  purple: '#A36BFF',
};

export interface FeatureSectionProps {
  eyebrow: string;
  title: string;
  children: ReactNode;
  accent: Accent;
  media: ReactNode;
  tags?: string[];
  href?: string;
  reverse?: boolean;
}

export function FeatureSection({ eyebrow, title, children, accent, media, tags, href, reverse }: FeatureSectionProps) {
  return (
    <div className="grid items-center gap-10 sm:gap-12 lg:grid-cols-12 lg:gap-16">
      <Reveal className={cn('lg:col-span-5', reverse && 'lg:order-2')}>
        <p className={cn('t-eyebrow flex items-center gap-2', accentText[accent])}>
          <Curl color={accentColor[accent]} className="size-5" />
          {eyebrow}
        </p>
        <h3 className="display t-feature mt-4">{title}</h3>
        <div className="t-lede mt-5 space-y-3 text-gl-ink-2">{children}</div>
        {tags && (
          <ul className="mt-7 flex flex-wrap gap-2" aria-label="Highlights">
            {tags.map((tag) => (
              <li key={tag} className="rounded-full bg-gl-mist px-3 py-1.5 text-[13px] font-medium text-gl-ink/80">
                {tag}
              </li>
            ))}
          </ul>
        )}
        {href && (
          <TextLink href={href} className="mt-7">
            Learn more<span className="sr-only"> about {eyebrow.toLowerCase()}</span>
          </TextLink>
        )}
      </Reveal>
      <Reveal className={cn('lg:col-span-7', reverse && 'lg:order-1')} delay={120}>
        {media}
      </Reveal>
    </div>
  );
}

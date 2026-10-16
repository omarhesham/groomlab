// SPDX-License-Identifier: Apache-2.0
import { ArrowDownToLine, Heart } from 'lucide-react';
import { cn } from '@/lib/cn';
import { siteConfig } from '@/site.config';

/** Pill buttons with squash and stretch (see `.squish` in global.css). */
export const pill = {
  base: 'squish inline-flex items-center justify-center gap-1.5 rounded-full font-medium focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-gl-blue',
  md: 'h-11 px-6 text-[0.95rem]',
  lg: 'h-[3.25rem] px-7 text-[1.0625rem]',
  primary:
    'bg-gl-blue-strong text-white shadow-[0_10px_24px_-10px_rgba(36,104,224,0.7)] hover:bg-[#1d5bcc] hover:shadow-[0_14px_30px_-10px_rgba(36,104,224,0.75)]',
};

/** The free download (every feature, any use) and the donation page. */
export function StoreButtons({
  className,
  align = 'center',
  tone = 'light',
}: {
  className?: string;
  align?: 'center' | 'start';
  /** The background the buttons sit on. */
  tone?: 'light' | 'dark';
}) {
  return (
    <div
      className={cn(
        'flex flex-col gap-3 sm:flex-row sm:gap-4',
        align === 'center' ? 'items-stretch justify-center sm:items-center' : 'items-stretch sm:items-center',
        className,
      )}
    >
      <a href={siteConfig.downloadUrl} className={cn(pill.base, pill.lg, pill.primary)}>
        Download free
        <ArrowDownToLine aria-hidden="true" className="size-4 opacity-80" strokeWidth={2.4} />
      </a>
      <a
        href={siteConfig.donateUrl}
        target="_blank"
        rel="noopener noreferrer"
        className={cn(
          pill.base,
          pill.lg,
          tone === 'light'
            ? 'bg-white text-gl-ink ring-1 ring-black/[0.08] hover:ring-black/[0.16]'
            : 'text-gl-blue-soft ring-1 ring-gl-blue-soft ring-inset hover:bg-gl-blue-soft hover:text-gl-night',
        )}
      >
        Support on Ko-fi
        <Heart aria-hidden="true" className="size-4 opacity-60" strokeWidth={2.4} />
      </a>
    </div>
  );
}

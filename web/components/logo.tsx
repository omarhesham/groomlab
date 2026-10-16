// SPDX-License-Identifier: Apache-2.0
import { cn } from '@/lib/cn';
import { siteConfig } from '@/site.config';

/**
 * The GroomLab mark: three strands rising from the scalp and bending the same way (red, yellow,
 * blue), as the standalone app draws it (hosts/app/icons/ui_logo.svg). `mono` draws it in the
 * current text color (white over the landing artwork) instead of the brand colors.
 */
export function LogoMark({ className, mono = false }: { className?: string; mono?: boolean }) {
  const color = (brand: string) => (mono ? 'currentColor' : brand);
  return (
    <svg
      // Drawn 4 units high in its box, so the strands' roots sit on the name's baseline.
      viewBox="0 4 30 30"
      fill="none"
      strokeWidth="3"
      strokeLinecap="round"
      aria-hidden="true"
      className={cn(
        'size-7 shrink-0 origin-bottom transition-transform duration-500 ease-[cubic-bezier(0.34,1.56,0.64,1)] group-hover:-rotate-6 group-hover:scale-105',
        className,
      )}
    >
      <path d="M7 25 C7 14, 10 8, 15 6" stroke={color('#FF4D4F')} />
      <path d="M13 25 C13 16, 16 11, 21 9" stroke={color('#FFC53D')} />
      <path d="M19 25 C19 18, 21 14, 25 13" stroke={color('#2F7BF6')} />
    </svg>
  );
}

export function Logo({ className, mono = false }: { className?: string; mono?: boolean }) {
  return (
    <span className={cn('group inline-flex items-center gap-2 font-semibold tracking-tight', className)}>
      <LogoMark mono={mono} />
      <span className="text-[1.05rem]">{siteConfig.name}</span>
    </span>
  );
}

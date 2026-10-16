// SPDX-License-Identifier: Apache-2.0
import Link from 'next/link';
import type { ReactNode } from 'react';
import { cn } from '@/lib/cn';

/** An Apple-style text link: blue, with a chevron that springs forward on hover. */
export function TextLink({
  href,
  children,
  className,
  tone = 'light',
}: {
  href: string;
  children: ReactNode;
  className?: string;
  tone?: 'light' | 'dark';
}) {
  return (
    <Link
      href={href}
      className={cn(
        'group/link inline-flex items-center gap-1 rounded-md text-[1.0625rem] font-medium hover:underline hover:underline-offset-4 focus-visible:outline-2 focus-visible:outline-offset-4',
        tone === 'light'
          ? 'text-gl-blue-strong focus-visible:outline-gl-blue'
          : 'text-gl-blue-soft focus-visible:outline-gl-blue-soft',
        className,
      )}
    >
      {children}
      <svg
        aria-hidden="true"
        viewBox="0 0 16 16"
        className="size-3.5 transition-transform duration-500 ease-spring group-hover/link:translate-x-1"
        fill="none"
      >
        <path d="M6 3.5 10.5 8 6 12.5" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
      </svg>
    </Link>
  );
}

// SPDX-License-Identifier: Apache-2.0
import Link from 'next/link';
import { Logo } from '@/components/logo';
import { pill } from '@/components/store-buttons';
import { cn } from '@/lib/cn';
import { siteConfig } from '@/site.config';

const links = [
  { href: '/#features', label: 'Features', mobile: false },
  { href: '/docs', label: 'Docs', mobile: true },
  { href: siteConfig.repoUrl, label: 'GitHub', mobile: false },
];

/** Frosted paper over the graph canvas, a hairline under it and a wire that grows as the page scrolls. */
export function SiteHeader() {
  return (
    <header className="sticky top-0 z-40 border-b border-black/[0.06] bg-white/80 text-gl-ink backdrop-blur-xl backdrop-saturate-[1.8]">
      <nav aria-label="Main" className="wrap flex h-12 items-center justify-between gap-4">
        <Link
          href="/"
          aria-label="GroomLab home"
          className="rounded-lg focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-gl-blue"
        >
          <Logo />
        </Link>
        <div className="flex items-center gap-1 sm:gap-3">
          <ul className="flex items-center">
            {links.map((link) => (
              <li key={link.href} className={link.mobile ? undefined : 'hidden sm:block'}>
                {link.href.startsWith('/') ? (
                  <Link
                    href={link.href}
                    className="rounded-full px-3 py-2 text-[13px] text-gl-ink/75 transition-colors hover:text-gl-ink focus-visible:outline-2 focus-visible:outline-gl-blue"
                  >
                    {link.label}
                  </Link>
                ) : (
                  <a
                    href={link.href}
                    className="rounded-full px-3 py-2 text-[13px] text-gl-ink/75 transition-colors hover:text-gl-ink focus-visible:outline-2 focus-visible:outline-gl-blue"
                  >
                    {link.label}
                  </a>
                )}
              </li>
            ))}
          </ul>
          <Link href="/download" className={cn(pill.base, pill.primary, 'h-8 px-4 text-[13px] shadow-none hover:shadow-none')}>
            Get GroomLab
          </Link>
        </div>
      </nav>
      {/* A wire in the mark's colors that grows along the header as the page scrolls. */}
      <svg
        aria-hidden="true"
        viewBox="0 0 1440 4"
        preserveAspectRatio="none"
        fill="none"
        className="scroll-hair-wrap absolute inset-x-0 -bottom-px h-[2px] w-full"
      >
        <defs>
          <linearGradient id="header-wire" x1="0" x2="1440" y1="0" y2="0" gradientUnits="userSpaceOnUse">
            <stop offset="0" stopColor="#FF4D4F" />
            <stop offset="0.5" stopColor="#FFC53D" />
            <stop offset="1" stopColor="#2F7BF6" />
          </linearGradient>
        </defs>
        <path d="M0 2 H1440" pathLength={1} className="strand scroll-hair" stroke="url(#header-wire)" strokeWidth="4" />
      </svg>
    </header>
  );
}

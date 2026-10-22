// SPDX-License-Identifier: Apache-2.0
import Link from 'next/link';
import { GitHubMark } from '@/components/github-mark';
import { Logo } from '@/components/logo';
import { button } from '@/components/store-buttons';
import { cn } from '@/lib/cn';
import { siteConfig } from '@/site.config';

const links = [
  { href: '/#features', label: 'Features', mobile: false },
  { href: '/#try', label: 'Try it', mobile: false },
  { href: '/docs', label: 'Docs', mobile: true },
];

/** Smoked glass over the dark page, a hairline under it and a wire that grows as the page scrolls. */
export function SiteHeader() {
  return (
    <header className="sticky top-0 z-40 border-b border-white/[0.07] bg-[#0b0c0f]/80 text-gl-ink backdrop-blur-xl backdrop-saturate-[1.6]">
      <nav aria-label="Main" className="wrap-wide flex h-16 items-center justify-between gap-4">
        <div className="flex items-center gap-3 sm:gap-8 lg:gap-12">
          <Link
            href="/"
            aria-label="GroomLab home"
            className="rounded-lg focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-gl-blue"
          >
            <Logo className="sm:[&>span:last-child]:text-[1.2rem]" />
          </Link>
          <ul className="flex items-center gap-1">
            {links.map((link) => (
              <li key={link.href} className={link.mobile ? undefined : 'hidden md:block'}>
                <Link
                  href={link.href}
                  className="rounded-lg px-3 py-2 text-[14.5px] text-gl-ink/80 transition-colors hover:text-gl-ink focus-visible:outline-2 focus-visible:outline-gl-blue"
                >
                  {link.label}
                </Link>
              </li>
            ))}
          </ul>
        </div>
        <div className="flex items-center gap-2 sm:gap-4">
          <a
            href={siteConfig.repoUrl}
            className="hidden items-center gap-2 rounded-lg px-3 py-2 text-[14.5px] text-gl-ink/80 transition-colors hover:text-gl-ink focus-visible:outline-2 focus-visible:outline-gl-blue sm:inline-flex"
          >
            <GitHubMark className="size-4" />
            GitHub
          </a>
          <Link href="/download" className={cn(button.base, button.primary, 'h-10 px-4 text-[14px] whitespace-nowrap shadow-none hover:shadow-none sm:px-5')}>
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

// SPDX-License-Identifier: Apache-2.0
'use client';

import Link from 'next/link';
import { useEffect, useState } from 'react';
import { Logo } from '@/components/logo';
import { pill } from '@/components/store-buttons';
import { cn } from '@/lib/cn';

const links = [
  { href: '/#features', label: 'Features', mobile: false },
  { href: '/docs', label: 'Docs', mobile: true },
];

/** True while the page's full-screen hero (#hero) is still under the header. */
function useOverHero() {
  const [over, setOver] = useState(false);
  useEffect(() => {
    const hero = document.getElementById('hero');
    if (!hero) return;
    const update = () => setOver(hero.getBoundingClientRect().bottom > 48);
    update();
    window.addEventListener('scroll', update, { passive: true });
    window.addEventListener('resize', update);
    return () => {
      window.removeEventListener('scroll', update);
      window.removeEventListener('resize', update);
    };
  }, []);
  return over;
}

export function SiteHeader() {
  const overHero = useOverHero();
  return (
    <header
      className={cn(
        'sticky top-0 z-40 transition-[background-color,color] duration-300',
        overHero ? 'bg-transparent text-white' : 'bg-white/80 text-gl-ink backdrop-blur-xl backdrop-saturate-[1.8]',
      )}
    >
      <nav aria-label="Main" className="wrap flex h-12 items-center justify-between gap-4">
        <Link
          href="/"
          aria-label="GroomLab home"
          className="rounded-lg focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-gl-blue"
        >
          <Logo mono={overHero} />
        </Link>
        <div className="flex items-center gap-1 sm:gap-3">
          <ul className="flex items-center">
            {links.map((link) => (
              <li key={link.href} className={link.mobile ? undefined : 'hidden sm:block'}>
                <Link
                  href={link.href}
                  className={cn(
                    'rounded-full px-3 py-2 text-[13px] transition-colors focus-visible:outline-2 focus-visible:outline-gl-blue',
                    overHero ? 'text-white/85 hover:text-white' : 'text-gl-ink/80 hover:text-gl-ink',
                  )}
                >
                  {link.label}
                </Link>
              </li>
            ))}
          </ul>
          <Link
            href="/download"
            className={cn(
              pill.base,
              'h-8 px-4 text-[13px] shadow-none hover:shadow-none',
              overHero ? 'bg-white text-[#5D779C] hover:bg-white/90' : pill.primary,
            )}
          >
            Get GroomLab
          </Link>
        </div>
      </nav>
      {/* A strand that grows along the header as the page scrolls. */}
      <svg
        aria-hidden="true"
        viewBox="0 0 1440 6"
        preserveAspectRatio="none"
        fill="none"
        className={cn(
          'scroll-hair-wrap absolute inset-x-0 -bottom-[3px] h-[6px] w-full transition-opacity duration-300',
          overHero && 'opacity-0',
        )}
      >
        <path
          d="M0 3 C 60 0, 120 6, 180 3 S 300 0, 360 3 S 480 6, 540 3 S 660 0, 720 3 S 840 6, 900 3 S 1020 0, 1080 3 S 1200 6, 1260 3 S 1380 0, 1440 3"
          pathLength={1}
          className="strand scroll-hair"
          stroke="#2F7BF6"
          strokeWidth="2"
          vectorEffect="non-scaling-stroke"
        />
      </svg>
    </header>
  );
}

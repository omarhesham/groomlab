// SPDX-License-Identifier: Apache-2.0
'use client';
import { useEffect, useRef } from 'react';

/**
 * A number that counts up to its value the first time it scrolls into view. The final text is
 * rendered on the server, so without scripts (or with reduced motion) it simply shows.
 */
export function CountUp({ value, decimals = 0, suffix = '' }: { value: number; decimals?: number; suffix?: string }) {
  const ref = useRef<HTMLSpanElement>(null);
  const text = (v: number) => `${v.toLocaleString('en-US', { minimumFractionDigits: decimals, maximumFractionDigits: decimals })}${suffix}`;

  useEffect(() => {
    const el = ref.current;
    if (!el || window.matchMedia('(prefers-reduced-motion: reduce)').matches) return;
    let raf = 0;
    const io = new IntersectionObserver(
      ([entry]) => {
        if (!entry.isIntersecting) return;
        io.disconnect();
        const start = performance.now();
        const tick = (now: number) => {
          const t = Math.min(1, (now - start) / 1300);
          const eased = 1 - Math.pow(1 - t, 4);
          el.textContent = text(value * eased);
          if (t < 1) raf = requestAnimationFrame(tick);
        };
        raf = requestAnimationFrame(tick);
      },
      { threshold: 0.6 },
    );
    io.observe(el);
    return () => {
      io.disconnect();
      cancelAnimationFrame(raf);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps -- the value is fixed for the page
  }, [value]);

  return (
    <span ref={ref} className="tabular-nums">
      {text(value)}
    </span>
  );
}

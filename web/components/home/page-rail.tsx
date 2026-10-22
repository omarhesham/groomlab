// SPDX-License-Identifier: Apache-2.0
'use client';
import { useEffect, useRef } from 'react';

/**
 * The page wire: one wire down the left margin that every section's tag branches off. It fills
 * in the mark's colors down to the middle of the screen as the page scrolls, a pulse riding its
 * tip. Wide screens only (there is no margin for it on smaller ones); with reduced motion it is
 * drawn in full and holds still.
 */
export function PageRail() {
  const railRef = useRef<HTMLDivElement>(null);
  const fillRef = useRef<HTMLDivElement>(null);
  const tipRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const rail = railRef.current;
    const fill = fillRef.current;
    const tip = tipRef.current;
    if (!rail || !fill || !tip) return;
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
      fill.style.transform = 'none';
      tip.style.opacity = '0';
      return;
    }
    let raf = 0;
    const update = () => {
      raf = 0;
      const r = rail.getBoundingClientRect();
      if (!r.height) return;
      const p = Math.max(0, Math.min(1, (window.innerHeight * 0.55 - r.top) / r.height));
      fill.style.transform = `scaleY(${p})`;
      tip.style.transform = `translate(-50%, ${p * r.height - 6}px)`;
      tip.style.opacity = p > 0 && p < 1 ? '1' : '0';
    };
    const schedule = () => {
      if (!raf) raf = requestAnimationFrame(update);
    };
    update();
    window.addEventListener('scroll', schedule, { passive: true });
    window.addEventListener('resize', schedule);
    return () => {
      cancelAnimationFrame(raf);
      window.removeEventListener('scroll', schedule);
      window.removeEventListener('resize', schedule);
    };
  }, []);

  return (
    <div
      ref={railRef}
      aria-hidden="true"
      className="pointer-events-none absolute top-0 bottom-0 left-[calc(50%-540px-64px)] hidden w-[2px] xl:block"
    >
      <div className="absolute inset-0 rounded-full bg-[#e4e4e9]" />
      <div
        ref={fillRef}
        className="absolute inset-0 origin-top rounded-full"
        style={{
          transform: 'scaleY(0)',
          background: 'linear-gradient(to bottom, #FF4D4F, #FFC53D 35%, #2F7BF6 70%, #1d1d1f)',
        }}
      />
      <div
        ref={tipRef}
        className="absolute top-0 left-1/2 size-3 rounded-full border-2 border-white bg-gl-blue opacity-0 shadow-[0_0_0_4px_rgba(47,123,246,0.16)] transition-opacity duration-300"
      />
    </div>
  );
}

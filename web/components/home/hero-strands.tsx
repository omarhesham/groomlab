// SPDX-License-Identifier: Apache-2.0
'use client';
import { useEffect, useRef, type CSSProperties, type Ref } from 'react';
import { cn } from '@/lib/cn';

/**
 * Two tufts of hair growing from behind the hero image, drawn in the logo's
 * style. They draw themselves on load and sway on their own (CSS); the pointer
 * combs them: they lean toward it, and brushing past flicks them on springs.
 */

interface StrandDef {
  d: string;
  color: string;
  width: number;
  /** Root x in the 100x100 box (roots sit on y = 100). */
  root: number;
  /** How much the strand bends to the pointer (taller strands bend more). */
  flex: number;
  sway: number;
  swayDur: number;
  delay: number;
}

const GRAY = '#d2d2d7';

/** A tuft curling to the right; the left one is its mirror image. */
const STRANDS: StrandDef[] = [
  { d: 'M31 100 C30 82 26 64 15 49', color: GRAY, width: 2.4, root: 31, flex: 0.8, sway: 2.6, swayDur: 5.2, delay: 0.25 },
  { d: 'M22 100 C22 72 28 46 52 33 C68 24 84 33 80 47 C77 58 62 57 63 46', color: '#2F7BF6', width: 5.2, root: 22, flex: 1, sway: 1.6, swayDur: 6.4, delay: 0.1 },
  { d: 'M36 100 C37 84 40 72 46 62', color: '#A36BFF', width: 2.6, root: 36, flex: 0.7, sway: 2.2, swayDur: 4.6, delay: 0.4 },
  { d: 'M42 100 C43 82 49 68 66 60', color: '#FFC53D', width: 5, root: 42, flex: 0.85, sway: 2, swayDur: 5.6, delay: 0.3 },
  { d: 'M52 100 C54 90 58 84 64 80', color: GRAY, width: 2.2, root: 52, flex: 0.6, sway: 2.8, swayDur: 4.2, delay: 0.5 },
  { d: 'M58 100 C61 89 68 82 81 79', color: '#FF4D4F', width: 5, root: 58, flex: 0.7, sway: 2.4, swayDur: 5, delay: 0.45 },
];

export function HeroStrands({ className }: { className?: string }) {
  const leftRef = useRef<SVGSVGElement>(null);
  const rightRef = useRef<SVGSVGElement>(null);

  useEffect(() => {
    const svgs = [leftRef.current, rightRef.current];
    if (!svgs[0] || !svgs[1]) return;
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) return;

    const n = STRANDS.length;
    const groups = svgs.map((svg) => Array.from(svg!.querySelectorAll<SVGGElement>('[data-strand]')));
    const angle = [new Float32Array(n), new Float32Array(n)];
    const vel = [new Float32Array(n), new Float32Array(n)];

    let px = 0;
    let py = 0;
    let lastX = NaN;
    let vx = 0;
    let active = false;
    let raf = 0;
    let visible = true;
    let last = performance.now();
    let idle = 0;

    const frame = (now: number) => {
      raf = 0;
      const dt = Math.min((now - last) / 1000, 1 / 30);
      last = now;
      let moving = false;

      for (let side = 0; side < 2; side++) {
        const svg = svgs[side]!;
        const rect = svg.getBoundingClientRect();
        const mirror = side === 0 ? -1 : 1;
        for (let i = 0; i < n; i++) {
          const s = STRANDS[i];
          // The strand's root and middle on screen (the left tuft is mirrored).
          const u = s.root / 100;
          const rootX = rect.left + rect.width * (mirror === 1 ? u : 1 - u);
          const midY = rect.top + rect.height * 0.55;
          let target = 0;
          let push = 0;
          if (active) {
            const dx = px - rootX;
            target = Math.max(-1, Math.min(1, dx / 700)) * 9 * s.flex;
            const r = Math.hypot(dx, py - midY);
            const reach = rect.width * 0.75 + 40;
            if (r < reach) {
              const f = 1 - r / reach;
              push = vx * 0.4 * f * f * s.flex;
            }
          }
          let a = angle[side][i];
          let w = vel[side][i];
          w += (-60 * (a - target) - 7 * w + push) * dt;
          a = Math.max(-28, Math.min(28, a + w * dt));
          angle[side][i] = a;
          vel[side][i] = w;
          if (Math.abs(a - target) > 0.02 || Math.abs(w) > 0.05) moving = true;
          // Screen rotation, flipped back inside the mirrored tuft.
          groups[side][i].style.rotate = `${(a * mirror).toFixed(2)}deg`;
        }
      }

      vx *= Math.pow(0.02, dt);
      idle += dt;
      if (visible && (moving || idle < 0.3)) raf = requestAnimationFrame(frame);
    };

    const kick = () => {
      idle = 0;
      if (!raf && visible) {
        last = performance.now();
        raf = requestAnimationFrame(frame);
      }
    };

    const section = svgs[1]!.closest('section');
    const onMove = (e: PointerEvent) => {
      const box = section?.getBoundingClientRect();
      active = !!box && e.clientY >= box.top && e.clientY <= box.bottom;
      const now = performance.now();
      if (!Number.isNaN(lastX)) {
        const inst = (e.clientX - lastX) / Math.max((now - last) / 1000, 1 / 120);
        vx += (Math.max(-3000, Math.min(3000, inst)) - vx) * 0.5;
      }
      lastX = e.clientX;
      px = e.clientX;
      py = e.clientY;
      kick();
    };
    const onLeave = () => {
      active = false;
      lastX = NaN;
      kick();
    };

    const io = new IntersectionObserver(([entry]) => {
      visible = entry.isIntersecting;
      if (visible) kick();
    });
    io.observe(svgs[1]!);
    window.addEventListener('pointermove', onMove, { passive: true });
    document.documentElement.addEventListener('pointerleave', onLeave);
    return () => {
      cancelAnimationFrame(raf);
      io.disconnect();
      window.removeEventListener('pointermove', onMove);
      document.documentElement.removeEventListener('pointerleave', onLeave);
    };
  }, []);

  return (
    <div aria-hidden="true" className={cn('pointer-events-none', className)}>
      <Tuft ref={leftRef} className="absolute bottom-0 left-[5%] w-[clamp(72px,13.5vw,190px)] translate-y-[12%] -scale-x-100" />
      <Tuft ref={rightRef} className="absolute right-[4%] bottom-0 translate-y-[12%]" />
    </div>
  );
}

function Tuft({ className, ref }: { className?: string; ref: Ref<SVGSVGElement> }) {
  return (
    <svg ref={ref} viewBox="0 0 100 100" fill="none" className={cn('aspect-square w-[clamp(84px,16vw,224px)] overflow-visible', className)}>
      {STRANDS.map((s, i) => (
        <g
          key={i}
          data-strand=""
          style={{ transformBox: 'view-box', transformOrigin: `${s.root}px 100px` }}
        >
          <g
            className="sway"
            style={
              {
                transformOrigin: `${s.root}px 100px`,
                '--sway': `${s.sway}deg`,
                '--sway-dur': `${s.swayDur}s`,
                '--sway-delay': `${-i * 0.9}s`,
              } as CSSProperties
            }
          >
            <path
              d={s.d}
              pathLength={1}
              className="strand draw-in"
              stroke={s.color}
              strokeWidth={s.width}
              style={{ '--d': `${0.35 + s.delay}s` } as CSSProperties}
            />
          </g>
        </g>
      ))}
    </svg>
  );
}

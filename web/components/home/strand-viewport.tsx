// SPDX-License-Identifier: Apache-2.0
'use client';
import { useEffect, useRef, useState } from 'react';
import { cn } from '@/lib/cn';
import type { GroomStats } from './groom-3d';
import type { Groom3D } from './strand-3d';
import { StrandRenderer, type StrandParams } from './strand-engine';

/** True when this browser can make a WebGL2 context (three.js needs one). */
function hasWebGL2() {
  try {
    const gl = document.createElement('canvas').getContext('webgl2');
    gl?.getExtension('WEBGL_lose_context')?.loseContext();
    return !!gl;
  } catch {
    return false;
  }
}

/**
 * The groom's viewport. It opens as a 2D canvas (cheap, no dependencies) and, once it comes near
 * the screen, fetches three.js and the 3D groom and fades them in over it: a head whose hair is
 * grown by the same params, turning slowly, that a drag orbits. Without WebGL2 the 2D canvas stays.
 * Both only animate while on screen and the tab is visible; with reduced motion each draws one
 * still frame per change. `live` lets the caller vary params per frame (the hero's breathing Curl);
 * it is read every frame, never re-rendered through React.
 */
export function StrandViewport({
  params,
  live,
  className,
  label,
  strandScale = 6,
  spin,
  pitch,
  fill,
  onStats,
}: {
  params: StrandParams;
  live?: (time: number) => Partial<StrandParams> | undefined;
  className?: string;
  label?: string;
  /** 3D strands per unit of density (fewer on phones). */
  strandScale?: number;
  /** Turntable speed, radians a second. */
  spin?: number;
  pitch?: number;
  fill?: number;
  onStats?: (stats: GroomStats) => void;
}) {
  const wrapRef = useRef<HTMLDivElement>(null);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const glRef = useRef<HTMLCanvasElement>(null);
  const paramsRef = useRef(params);
  const liveRef = useRef(live);
  const statsRef = useRef(onStats);
  const redrawRef = useRef<() => void>(() => {});
  const groomRef = useRef<Groom3D | null>(null);
  /** When the 2D canvas was first seen (ms), so the 3D one knows whether to grow in again. */
  const seenRef = useRef(-1);
  const [three, setThree] = useState(false);

  useEffect(() => {
    paramsRef.current = params;
    liveRef.current = live;
    statsRef.current = onStats;
    redrawRef.current();
    groomRef.current?.invalidate();
  }, [params, live, onStats]);

  // The 2D canvas: the first paint, and the fallback.
  useEffect(() => {
    const canvas = canvasRef.current;
    const ctx = canvas?.getContext('2d');
    if (!canvas || !ctx) return;
    const renderer = new StrandRenderer(paramsRef.current.seed);
    const still = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    let width = 0;
    let height = 0;
    let visible = false;
    let raf = 0;
    let start = -1;

    const paint = (now: number) => {
      if (!width || !height || groomRef.current) return;
      if (start < 0) start = now;
      if (seenRef.current < 0) seenRef.current = now;
      const t = (now - start) / 1000;
      const grow = still ? 1 : 1 - Math.pow(1 - Math.min(1, t / 1.6), 3);
      const extra = liveRef.current?.(t);
      const p = extra ? { ...paramsRef.current, ...extra } : paramsRef.current;
      renderer.draw(ctx, p, { width, height, time: t, grow, sway: !still });
    };

    const loop = (now: number) => {
      raf = 0;
      paint(now);
      if (visible && !still && !document.hidden && !groomRef.current) raf = requestAnimationFrame(loop);
    };
    const kick = () => {
      if (!raf && visible) raf = requestAnimationFrame(loop);
    };
    redrawRef.current = kick;

    const resize = () => {
      const rect = canvas.getBoundingClientRect();
      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      width = rect.width;
      height = rect.height;
      canvas.width = Math.round(width * dpr);
      canvas.height = Math.round(height * dpr);
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      kick();
    };
    const ro = new ResizeObserver(resize);
    ro.observe(canvas);
    const io = new IntersectionObserver(
      ([entry]) => {
        visible = entry.isIntersecting;
        kick();
      },
      { rootMargin: '80px' },
    );
    io.observe(canvas);
    const onVisibility = () => kick();
    document.addEventListener('visibilitychange', onVisibility);
    return () => {
      ro.disconnect();
      io.disconnect();
      document.removeEventListener('visibilitychange', onVisibility);
      cancelAnimationFrame(raf);
      redrawRef.current = () => {};
    };
  }, []);

  // The 3D groom, fetched when the viewport comes within a screen or so of view.
  useEffect(() => {
    const wrap = wrapRef.current;
    const gl = glRef.current;
    if (!wrap || !gl) return;
    const still = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    const phone = window.matchMedia('(max-width: 640px)').matches;
    let disposed = false;
    let loading = false;
    let onScreen = false;
    let groom: Groom3D | null = null;

    const activate = () => groom?.setActive(onScreen && !document.hidden);
    const size = () => {
      const r = wrap.getBoundingClientRect();
      groom?.resize(r.width, r.height);
    };
    const load = () => {
      if (loading) return;
      loading = true;
      if (!hasWebGL2()) return;
      import('./strand-3d')
        .then(({ mountGroom3D }) => {
          if (disposed) return;
          const params = (t: number) => {
            const extra = liveRef.current?.(t);
            return extra ? { ...paramsRef.current, ...extra } : paramsRef.current;
          };
          groom = mountGroom3D(gl, {
            still,
            spin,
            pitch,
            fill,
            sway: 0.6,
            params,
            strandScale: strandScale * (phone ? 0.7 : 1),
            onStats: (s) => statsRef.current?.(s),
          });
          if (!groom) return;
          groomRef.current = groom;
          size();
          // Grow in, unless the 2D strands already have for a while.
          const seen = seenRef.current;
          if (seen < 0 || performance.now() - seen < 600) groom.grow(1.6);
          activate();
          setThree(true);
        })
        .catch(() => {});
    };

    const near = new IntersectionObserver(([e]) => e.isIntersecting && load(), { rootMargin: '600px 0px' });
    near.observe(wrap);
    const io = new IntersectionObserver(
      ([e]) => {
        onScreen = e.isIntersecting;
        activate();
      },
      { rootMargin: '40px 0px' },
    );
    io.observe(wrap);
    const ro = new ResizeObserver(size);
    ro.observe(wrap);
    document.addEventListener('visibilitychange', activate);
    return () => {
      disposed = true;
      near.disconnect();
      io.disconnect();
      ro.disconnect();
      document.removeEventListener('visibilitychange', activate);
      groom?.dispose();
      groomRef.current = null;
    };
    // The 3D options are fixed for the viewport's life.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <div
      ref={wrapRef}
      role={label ? 'img' : undefined}
      aria-label={label}
      aria-hidden={label ? undefined : true}
      className={cn('relative size-full', className)}
    >
      <canvas
        ref={canvasRef}
        aria-hidden="true"
        className={cn('absolute inset-0 block size-full transition-opacity duration-500', three && 'opacity-0')}
      />
      <canvas
        ref={glRef}
        aria-hidden="true"
        // The head fades out toward the bottom, like a bust, so the hair holds the eye.
        style={{ touchAction: 'pan-y', maskImage: 'linear-gradient(to bottom, #000 58%, transparent 94%)' }}
        className={cn(
          'absolute inset-0 block size-full cursor-grab opacity-0 transition-opacity duration-500',
          three && 'opacity-100',
          !three && 'pointer-events-none',
        )}
      />
    </div>
  );
}

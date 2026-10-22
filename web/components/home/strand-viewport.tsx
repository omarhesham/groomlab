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
 * The groom's viewport. Once it comes near the screen it fetches three.js and the 3D groom and
 * grows it in: a head whose hair is grown by the params, turning slowly, that a drag orbits.
 * Without WebGL2 (or when the 3D fails) a 2D canvas draws the same graph instead.
 * Both only animate while on screen and the tab is visible; with reduced motion each draws one
 * still frame per change. `live` lets the caller vary params per frame (the hero's breathing Curl);
 * it is read every frame, never re-rendered through React.
 */
export function StrandViewport({
  params,
  live,
  className,
  label,
  strandScale = 10,
  spin,
  pitch,
  fill,
  yaw,
  skin,
  framing,
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
  /** Where the turntable starts, radians (0 faces the camera). */
  yaw?: number;
  /** The head's clay color, sRGB 0 to 1. */
  skin?: number[];
  /** The sphere the camera frames, in head units. */
  framing?: [[number, number, number], number];
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
  /** Whether the 2D canvas draws: only without WebGL2 or when the 3D groom failed. */
  const flatRef = useRef(false);
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
    flatRef.current = !hasWebGL2();
    const still = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    let width = 0;
    let height = 0;
    let visible = false;
    let raf = 0;
    let start = -1;

    const paint = (now: number) => {
      if (!width || !height || groomRef.current || !flatRef.current) return;
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
      if (flatRef.current) return;
      const fail = () => {
        flatRef.current = true;
        redrawRef.current();
      };
      import('./strand-3d')
        .then(({ mountGroom3D }) => {
          if (disposed) return null;
          const params = (t: number) => {
            const extra = liveRef.current?.(still ? 0 : t);
            return extra ? { ...paramsRef.current, ...extra } : paramsRef.current;
          };
          return mountGroom3D(gl, {
            still,
            spin,
            pitch,
            fill,
            yaw,
            skin,
            framing,
            sway: 0.6,
            params,
            strandScale: strandScale * (phone ? 0.55 : 1),
            onStats: (s) => statsRef.current?.(s),
          });
        })
        .then((g) => {
          if (!g) {
            if (!disposed) fail();
            return;
          }
          if (disposed) {
            g.dispose();
            return;
          }
          groom = g;
          groomRef.current = groom;
          size();
          // Grow in, unless the 2D strands already have for a while.
          const seen = seenRef.current;
          if (seen < 0 || performance.now() - seen < 600) groom.grow(1.6);
          activate();
          setThree(true);
        })
        .catch(fail);
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
        style={{ touchAction: 'pan-y' }}
        className={cn(
          'absolute inset-0 block size-full cursor-grab opacity-0 transition-opacity duration-500',
          three && 'opacity-100',
          !three && 'pointer-events-none',
        )}
      />
    </div>
  );
}

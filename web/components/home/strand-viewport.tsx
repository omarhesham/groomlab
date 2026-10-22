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
 * The groom's viewport. Once it comes near the screen it fetches three.js and the 3D groom (grown in
 * a worker): a head whose hair is grown by the params, standing still, that a drag orbits. Without
 * WebGL2 (or when the 3D fails) a 2D canvas draws the same graph instead. Neither animates: each
 * draws a frame when its params change, when it is resized or shown, and while it is dragged.
 */
export function StrandViewport({
  params,
  className,
  label,
  strandScale = 10,
  pitch,
  fill,
  yaw,
  skin,
  framing,
  onStats,
}: {
  params: StrandParams;
  className?: string;
  label?: string;
  /** 3D strands per unit of density (fewer on phones). */
  strandScale?: number;
  pitch?: number;
  fill?: number;
  /** Which way the head faces, radians (0 faces the camera). */
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
  const statsRef = useRef(onStats);
  const redrawRef = useRef<() => void>(() => {});
  const groomRef = useRef<Groom3D | null>(null);
  /** Whether the 2D canvas draws: only without WebGL2 or when the 3D groom failed. */
  const flatRef = useRef(false);
  const [three, setThree] = useState(false);

  useEffect(() => {
    paramsRef.current = params;
    statsRef.current = onStats;
    redrawRef.current();
    groomRef.current?.update(params);
  }, [params, onStats]);

  // The 2D canvas: the fallback, one still frame per change.
  useEffect(() => {
    const canvas = canvasRef.current;
    const ctx = canvas?.getContext('2d');
    if (!canvas || !ctx) return;
    const renderer = new StrandRenderer(paramsRef.current.seed);
    flatRef.current = !hasWebGL2();
    let width = 0;
    let height = 0;
    let visible = false;
    let raf = 0;

    const loop = () => {
      raf = 0;
      if (!width || !height || groomRef.current || !flatRef.current || document.hidden) return;
      renderer.draw(ctx, paramsRef.current, { width, height, time: 0, grow: 1, sway: false });
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
          return mountGroom3D(gl, {
            still,
            pitch,
            fill,
            yaw,
            skin,
            framing,
            params: paramsRef.current,
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
          // Params that changed while it first grew.
          groom.update(paramsRef.current);
          size();
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

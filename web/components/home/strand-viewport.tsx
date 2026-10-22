// SPDX-License-Identifier: Apache-2.0
'use client';
import { useEffect, useRef } from 'react';
import { cn } from '@/lib/cn';
import { StrandRenderer, type StrandParams } from './strand-engine';

/**
 * A canvas that draws the procedural groom. It only animates while on screen (and the tab is
 * visible): the strands grow in the first time it is seen, then sway. With reduced motion it draws
 * one still frame whenever its params change. `live` lets the caller vary params per frame (the
 * hero's breathing Curl); it is read every frame, never re-rendered through React.
 */
export function StrandViewport({
  params,
  live,
  className,
  label,
}: {
  params: StrandParams;
  live?: (time: number) => Partial<StrandParams> | undefined;
  className?: string;
  label?: string;
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const paramsRef = useRef(params);
  const liveRef = useRef(live);
  const redrawRef = useRef<() => void>(() => {});

  useEffect(() => {
    paramsRef.current = params;
    liveRef.current = live;
    redrawRef.current();
  }, [params, live]);

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
      if (!width || !height) return;
      if (start < 0) start = now;
      const t = (now - start) / 1000;
      const grow = still ? 1 : 1 - Math.pow(1 - Math.min(1, t / 1.6), 3);
      const extra = liveRef.current?.(t);
      const p = extra ? { ...paramsRef.current, ...extra } : paramsRef.current;
      renderer.draw(ctx, p, { width, height, time: t, grow, sway: !still });
    };

    const loop = (now: number) => {
      raf = 0;
      paint(now);
      if (visible && !still && !document.hidden) raf = requestAnimationFrame(loop);
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

  return (
    <canvas
      ref={canvasRef}
      role={label ? 'img' : undefined}
      aria-label={label}
      aria-hidden={label ? undefined : true}
      className={cn('block size-full', className)}
    />
  );
}

// SPDX-License-Identifier: Apache-2.0
'use client';
import { useEffect, useRef, useState } from 'react';
import { cn } from '@/lib/cn';
import type { EngineEdits } from './engine-grooms';
import type { EngineOptions, Groom3D, GroomStats } from './strand-3d';

/**
 * The groom's viewport. Once it comes near the screen it fetches three.js and the bare head, standing
 * still, that a drag orbits; the real GroomLab engine (WebAssembly, in a worker) then grows the
 * `engine` groom with `edits` on it. Nothing animates: it draws a frame when its edits change, when
 * it is resized or shown, and while it is dragged.
 */
export function StrandViewport({
  className,
  label,
  pitch,
  fill,
  yaw,
  skin,
  framing,
  showGuides = false,
  onStats,
  engine,
  edits,
}: {
  className?: string;
  label?: string;
  pitch?: number;
  fill?: number;
  /** Which way the head faces, radians (0 faces the camera). */
  yaw?: number;
  /** The head's clay color, sRGB 0 to 1. */
  skin?: number[];
  /** The sphere the camera frames, in head units. */
  framing?: [[number, number, number], number];
  showGuides?: boolean;
  onStats?: (stats: GroomStats) => void;
  /** The real groom (fixed for the viewport's life) and its callbacks. */
  engine: Omit<EngineOptions, 'edits' | 'scale'>;
  /** Edits of the engine's groom: param values and bypassed nodes. */
  edits?: EngineEdits;
}) {
  const wrapRef = useRef<HTMLDivElement>(null);
  const glRef = useRef<HTMLCanvasElement>(null);
  const statsRef = useRef(onStats);
  const engineRef = useRef(engine);
  const editsRef = useRef(edits);
  const guidesRef = useRef(showGuides);
  const groomRef = useRef<Groom3D | null>(null);
  const [three, setThree] = useState(false);

  useEffect(() => {
    statsRef.current = onStats;
  }, [onStats]);

  useEffect(() => {
    guidesRef.current = showGuides;
    groomRef.current?.showGuides(showGuides);
  }, [showGuides]);

  useEffect(() => {
    engineRef.current = engine;
    editsRef.current = edits;
    if (edits) groomRef.current?.edit(edits);
  }, [engine, edits]);

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
            showGuides: guidesRef.current,
            onStats: (s) => statsRef.current?.(s),
            engine: {
              source: engineRef.current.source,
              edits: editsRef.current ?? {},
              scale: phone ? 0.55 : 1,
              onControls: (c) => engineRef.current.onControls?.(c),
              onReady: () => engineRef.current.onReady?.(),
            },
          });
        })
        .then((g) => {
          if (!g) return;
          if (disposed) {
            g.dispose();
            return;
          }
          groom = g;
          groomRef.current = groom;
          // Edits and the guides' toggle that changed while it loaded.
          if (editsRef.current) groom.edit(editsRef.current);
          groom.showGuides(guidesRef.current);
          size();
          activate();
          setThree(true);
        })
        .catch((err) => console.warn('GroomLab viewport failed:', err));
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

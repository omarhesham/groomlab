// SPDX-License-Identifier: Apache-2.0
/*
 * The lazily loaded half of StrandViewport: a groom wired to the three.js viewer, on the Manny head.
 * This module (and three.js with it) is only fetched when a viewport scrolls near the screen; the
 * head mesh (a few kB) is fetched with it, once per page. First it draws the procedural groom
 * (groom-3d.ts), at once; with an `engine` source it then fetches the real GroomLab engine
 * (home-engine.ts: WebAssembly, in a worker) when the page is idle and swaps in the engine's
 * strands, scalp and guides once they come, in the same canvas. Either stands still: it is grown
 * again only when its params or edits change (at most once a frame, the latest), and nothing renders
 * unless it is dragged, resized or changed.
 */
import type { GroomControl } from '@/lib/groomlab-wasm';
import type { EngineEdits, EngineSource } from './engine-grooms';
import { MAX_STRANDS, ProceduralGroom, type GroomStats } from './groom-3d';
import { loadHead } from './head-mesh';
import type { EngineGroom } from './home-engine';
import type { StrandData } from './strand-data';
import type { StrandParams } from './strand-engine';
import { createStrandViewer, type ViewerOptions } from './strand-three';

/** The real groom a viewport shows once the engine has grown it. */
export interface EngineOptions {
  source: EngineSource;
  edits: EngineEdits;
  onControls?: (controls: GroomControl[]) => void;
  /** Share of the source's strands to draw (phones: fewer). */
  scale?: number;
  /** Called once the engine's strands are on screen. */
  onReady?: () => void;
}

/**
 * The page's engine timings, for its perf probe (window.__groomlabHome): per slot, milestones as
 * milliseconds since the page started, and per evaluation the engine's time, the worker's round trip
 * and the time the page's thread took to hand the result to the viewer.
 */
type Timings = Record<string, Record<string, number | number[]>>;
function record(slot: string, name: string, ms = performance.now()) {
  const w = window as unknown as { __groomlabHome?: Timings };
  const t = ((w.__groomlabHome ??= {})[slot] ??= {});
  if (name === 'evaluateMs' || name === 'roundTripMs' || name === 'applyMs' || name === 'paintMs') ((t[name] ??= []) as number[]).push(Math.round(ms * 10) / 10);
  else t[name] = Math.round(ms);
}

export interface Groom3DOptions extends Omit<ViewerOptions, 'onFrame' | 'spin' | 'sway'> {
  params: StrandParams;
  /** Strands per unit of Scatter's density. */
  strandScale: number;
  /** The head's clay color, sRGB 0 to 1. */
  skin?: number[];
  /** The sphere the camera frames: center and radius, in head units (the cranium ~1 across). */
  framing?: [[number, number, number], number];
  onStats?: (stats: GroomStats) => void;
  /** The real groom to swap in, grown by the engine. */
  engine?: EngineOptions;
}

export interface Groom3D {
  /** Grows the groom again with these params, on the next frame, if they change it. */
  update(params: StrandParams): void;
  /** The engine's groom: these edits (sent once it has loaded). */
  edit(edits: EngineEdits): void;
  setActive(active: boolean): void;
  resize(width: number, height: number): void;
  invalidate(): void;
  dispose(): void;
}

/** The 3D groom on `canvas`, once its head has loaded; null without WebGL. */
export async function mountGroom3D(canvas: HTMLCanvasElement, opts: Groom3DOptions): Promise<Groom3D | null> {
  const groom = new ProceduralGroom();
  const head = await loadHead();
  if (head) groom.setHead(head);
  const viewer = createStrandViewer(canvas, { ...opts, spin: 0, sway: 0 });
  if (!viewer) return null;
  // The head and the hair over it, a little above center.
  const [center, radius] = opts.framing ?? [[0, 0.1, 0], 1.45];
  viewer.setFraming(center, radius);

  let shapeKey = '';
  let guidesKey = '';
  let strands = -1;
  let params = opts.params;
  let raf = 0;
  /** Once the engine's strands are in: the procedural groom stops, only the guides follow params. */
  let real = false;
  let realGuides: StrandData | null = null;
  let shownGuides: StrandData | null | undefined;
  const showGuides = () => {
    const g = params.showGuides ? realGuides : null;
    if (g !== shownGuides) {
      shownGuides = g;
      viewer.setGuides(g);
    }
  };
  const sync = () => {
    raf = 0;
    if (real) return showGuides();
    const p = params;
    const count = Math.min(MAX_STRANDS, Math.round(p.density * opts.strandScale));
    const key = [count, p.clump, p.clumpCount, p.curl, p.curlFrequency, p.noise, p.seed, p.length ?? 1, p.volume ?? 0].join();
    if (key !== shapeKey) {
      shapeKey = key;
      viewer.setStrands(groom.build(p, count));
    }
    const colors = groom.colorize(p);
    if (colors) {
      viewer.setStrandColors(colors);
      viewer.setScalp(groom.scalp(p, opts.skin));
    }
    // The guides change with the seed and with the points per strand (Curl's on and frequency).
    const gk = p.showGuides ? `${p.seed}:${p.curl > 0}:${p.curlFrequency}:${p.volume ?? 0}:${p.length ?? 1}` : '';
    if (gk !== guidesKey) {
      guidesKey = gk;
      viewer.setGuides(p.showGuides ? groom.guides() : null);
    }
    const stats = groom.stats();
    if (stats.strands !== strands) {
      strands = stats.strands;
      opts.onStats?.(stats);
    }
  };
  sync();

  // The engine, once the page is idle: its worker and WebAssembly are fetched then, once a page.
  let engine: EngineGroom | null = null;
  let disposed = false;
  let edits = opts.engine?.edits ?? {};
  let idle = 0;
  const startEngine = () => {
    const e = opts.engine;
    if (!e || disposed) return;
    const slot = e.source.slot;
    record(slot, 'engineAsked');
    import('./home-engine')
      .then(({ attachEngineGroom }) => {
        if (disposed) return;
        engine = attachEngineGroom({
          source: e.source,
          strandScale: e.scale ?? 1,
          skin: opts.skin ?? [0.7, 0.68, 0.66],
          edits,
          onControls: e.onControls,
          onTiming: (name, ms) => record(slot, name, ms),
          onFail: (err) => console.warn(`GroomLab engine (${slot}): staying procedural:`, err),
          onView(view) {
            if (disposed) return;
            if (!real) {
              real = true;
              cancelAnimationFrame(raf);
              raf = 0;
              record(slot, 'onScreen');
              e.onReady?.();
            }
            const t0 = performance.now();
            viewer.setStrands(view.strands);
            if (view.scalp) viewer.setScalp(view.scalp);
            realGuides = view.guides;
            shownGuides = undefined;
            showGuides();
            record(slot, 'applyMs', performance.now() - t0);
            strands = view.stats.strands;
            opts.onStats?.({ strands: view.stats.strands, guides: view.stats.guides });
          },
        });
      })
      .catch((err) => console.warn('GroomLab engine: staying procedural:', err));
  };
  if (opts.engine) {
    const w = window as Window & { requestIdleCallback?: (cb: () => void, o?: { timeout: number }) => number };
    if (w.requestIdleCallback) idle = w.requestIdleCallback(startEngine, { timeout: 1200 });
    else idle = window.setTimeout(startEngine, 200);
  }

  return {
    update(p) {
      if (p === params) return;
      params = p;
      // A slider fires faster than frames: grow once a frame, with the latest params.
      if (!raf) raf = requestAnimationFrame(sync);
    },
    edit(e) {
      edits = e;
      engine?.edit(e);
    },
    setActive: viewer.setActive,
    resize: viewer.resize,
    invalidate: viewer.invalidate,
    dispose() {
      disposed = true;
      const w = window as Window & { cancelIdleCallback?: (id: number) => void };
      w.cancelIdleCallback?.(idle);
      clearTimeout(idle);
      engine?.dispose();
      cancelAnimationFrame(raf);
      viewer.dispose();
    },
  };
}

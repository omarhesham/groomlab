// SPDX-License-Identifier: Apache-2.0
/*
 * The lazily loaded half of StrandViewport: the procedural groom wired to the three.js viewer, on
 * the Manny head. This module (and three.js with it) is only fetched when a viewport scrolls near
 * the screen; the head mesh (a few kB) is fetched with it, once per page. The groom stands still:
 * it is grown once, and again only when its params change (at most once a frame, the latest
 * params), and nothing renders unless it is dragged, resized or changed.
 */
import { MAX_STRANDS, ProceduralGroom, type GroomStats } from './groom-3d';
import { loadHead } from './head-mesh';
import type { StrandParams } from './strand-engine';
import { createStrandViewer, type ViewerOptions } from './strand-three';

export interface Groom3DOptions extends Omit<ViewerOptions, 'onFrame' | 'spin' | 'sway'> {
  params: StrandParams;
  /** Strands per unit of Scatter's density. */
  strandScale: number;
  /** The head's clay color, sRGB 0 to 1. */
  skin?: number[];
  /** The sphere the camera frames: center and radius, in head units (the cranium ~1 across). */
  framing?: [[number, number, number], number];
  onStats?: (stats: GroomStats) => void;
}

export interface Groom3D {
  /** Grows the groom again with these params, on the next frame, if they change it. */
  update(params: StrandParams): void;
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
  const sync = () => {
    raf = 0;
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

  return {
    update(p) {
      if (p === params) return;
      params = p;
      // A slider fires faster than frames: grow once a frame, with the latest params.
      if (!raf) raf = requestAnimationFrame(sync);
    },
    setActive: viewer.setActive,
    resize: viewer.resize,
    invalidate: viewer.invalidate,
    dispose() {
      cancelAnimationFrame(raf);
      viewer.dispose();
    },
  };
}

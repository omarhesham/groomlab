// SPDX-License-Identifier: Apache-2.0
/*
 * The lazily loaded half of StrandViewport: the procedural groom wired to the three.js viewer. This
 * module (and three.js with it) is only fetched when a viewport scrolls near the screen.
 */
import { MAX_STRANDS, ProceduralGroom, type GroomStats } from './groom-3d';
import type { StrandParams } from './strand-engine';
import { createStrandViewer, type StrandViewer, type ViewerOptions } from './strand-three';

export interface Groom3DOptions extends Omit<ViewerOptions, 'onFrame'> {
  /** The params to draw at this time (seconds); read every frame. */
  params: (time: number) => StrandParams;
  /** Strands per unit of Scatter's density. */
  strandScale: number;
  onStats?: (stats: GroomStats) => void;
}

export interface Groom3D {
  setActive(active: boolean): void;
  resize(width: number, height: number): void;
  invalidate(): void;
  grow(seconds: number): void;
  dispose(): void;
}

export function mountGroom3D(canvas: HTMLCanvasElement, opts: Groom3DOptions): Groom3D | null {
  const groom = new ProceduralGroom();
  let shapeKey = '';
  let guidesKey = '';
  let strands = -1;
  let viewer: StrandViewer | null = null;
  const sync = (time: number) => {
    if (!viewer) return;
    const p = opts.params(time);
    const count = Math.min(MAX_STRANDS, Math.round(p.density * opts.strandScale));
    // Quantized, so a param that drifts every frame (the hero's Curl) regrows a few times a second.
    const q = (v: number) => Math.round(v * 200) / 200;
    const key = [count, q(p.clump), p.clumpCount, q(p.curl), p.curlFrequency, q(p.noise), p.seed].join();
    if (key !== shapeKey) {
      shapeKey = key;
      viewer.setStrands(groom.build(p, count));
    }
    const colors = groom.colorize(p);
    if (colors) {
      viewer.setStrandColors(colors);
      viewer.setScalp(groom.scalp(p));
    }
    // The guides change with the seed and with the points per strand (Curl's on and frequency).
    const gk = p.showGuides ? `${p.seed}:${p.curl > 0}:${p.curlFrequency}` : '';
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
  viewer = createStrandViewer(canvas, { ...opts, onFrame: sync });
  if (!viewer) return null;
  // The head and the hair over it, a little above center.
  viewer.setFraming([0, 0.28, 0], 1.32);
  sync(0);
  return viewer;
}

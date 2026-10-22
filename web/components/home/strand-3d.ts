// SPDX-License-Identifier: Apache-2.0
/*
 * The lazily loaded half of StrandViewport: the procedural groom wired to the three.js viewer, on
 * the Manny head. This module (and three.js with it) is only fetched when a viewport scrolls near
 * the screen; the head mesh (a few kB) is fetched with it, once per page.
 */
import { MAX_STRANDS, ProceduralGroom, type GroomStats } from './groom-3d';
import { loadHead } from './head-mesh';
import type { StrandParams } from './strand-engine';
import { createStrandViewer, type StrandViewer, type ViewerOptions } from './strand-three';

export interface Groom3DOptions extends Omit<ViewerOptions, 'onFrame'> {
  /** The params to draw at this time (seconds); read every frame. */
  params: (time: number) => StrandParams;
  /** Strands per unit of Scatter's density. */
  strandScale: number;
  /** The head's clay color, sRGB 0 to 1. */
  skin?: number[];
  /** The sphere the camera frames: center and radius, in head units (the cranium ~1 across). */
  framing?: [[number, number, number], number];
  onStats?: (stats: GroomStats) => void;
}

export interface Groom3D {
  setActive(active: boolean): void;
  resize(width: number, height: number): void;
  invalidate(): void;
  grow(seconds: number): void;
  dispose(): void;
}

/** The 3D groom on `canvas`, once its head has loaded; null without WebGL. */
export async function mountGroom3D(canvas: HTMLCanvasElement, opts: Groom3DOptions): Promise<Groom3D | null> {
  const groom = new ProceduralGroom();
  const head = await loadHead();
  if (head) groom.setHead(head);
  let shapeKey = '';
  let guidesKey = '';
  let strands = -1;
  let lastBuild = -1e9;
  let retry = 0;
  let viewer: StrandViewer | null = null;
  const sync = (time: number) => {
    if (!viewer) return;
    const p = opts.params(time);
    const count = Math.min(MAX_STRANDS, Math.round(p.density * opts.strandScale));
    // Quantized, so a param that drifts every frame (the hero's Curl) regrows a few times a second.
    const q = (v: number) => Math.round(v * 100) / 100;
    const key = [count, q(p.clump), p.clumpCount, q(p.curl), p.curlFrequency, q(p.noise), p.seed, q(p.length ?? 1), p.volume ?? 0].join();
    if (key !== shapeKey) {
      // At most a dozen regrows a second: a frame that comes sooner leaves it to a later one.
      const now = performance.now();
      if (now - lastBuild < 80) {
        if (!retry) retry = window.setTimeout(() => ((retry = 0), viewer?.invalidate()), 90);
      } else {
        lastBuild = now;
        shapeKey = key;
        viewer.setStrands(groom.build(p, count));
      }
    }
    const colors = groom.colorize(p);
    if (colors) {
      viewer.setStrandColors(colors);
      viewer.setScalp(groom.scalp(p, opts.skin));
    }
    // The guides change with the seed and with the points per strand (Curl's on and frequency).
    const gk = p.showGuides ? `${p.seed}:${p.curl > 0}:${p.curlFrequency}:${p.volume ?? 0}` : '';
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
  const [center, radius] = opts.framing ?? [[0, 0.1, 0], 1.45];
  viewer.setFraming(center, radius);
  sync(0);
  const dispose = viewer.dispose;
  viewer.dispose = () => {
    clearTimeout(retry);
    dispose();
  };
  return viewer;
}

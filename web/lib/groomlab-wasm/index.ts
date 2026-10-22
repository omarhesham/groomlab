// SPDX-License-Identifier: Apache-2.0
/*
 * The GroomLab engine in the browser: the core built to WebAssembly (engine/web, docs/WEB.md),
 * running in a worker (public/wasm/groomlab-worker.js) so evaluating never blocks the page. Load a
 * .groom, list its sliders, set them, evaluate at a preview density and save the edited file.
 * Nothing here is fetched until createGroomEngine() is called: pages that never ask never pay.
 */
import type { RootMesh, StrandData } from '@/components/home/strand-data';

/** One slider: a Control node's value, or a node param exposed by the groom. */
export interface GroomControl {
  /** "control:<name>" or "param:<node>.<param>": what setParam takes. */
  id: string;
  /** control: a Control node; float or int: a node param. */
  kind: 'control' | 'float' | 'int';
  label: string;
  /** What it belongs to: "Controls", or the node ("Clump (clump)"). */
  group: string;
  node: string;
  param: string;
  min: number;
  max: number;
  step: number;
  default: number;
  value: number;
  /** The param's unit: "length", "width", "density", "angle"... ("" none). */
  unit: string;
  description: string;
}

export interface GroomLoaded {
  /** The engine's version (GroomLab's). */
  version: string;
  loadMs: number;
  controls: GroomControl[];
  scalp: RootMesh;
}

export interface GroomEvaluation {
  strands: StrandData;
  /** The guides as drawn (the flat guides, else the hierarchy's); null without any. */
  guides: StrandData | null;
  /** The engine's own time for the evaluation, and the worker's round trip (copies included). */
  evaluateMs: number;
  totalMs: number;
  /** Why the evaluation was partial, line by line (empty: it was not). */
  errors: string[];
  /** Milliseconds per node in this evaluation (nodes from cache take none). */
  nodes: { node: string; ms: number }[];
}

export interface GroomEngine {
  /** A .groom (its scalp inside), or a preset's graph with an .obj to grow on. */
  loadGroom(groom: string | ArrayBuffer | Uint8Array, mesh?: string | ArrayBuffer | Uint8Array): Promise<GroomLoaded>;
  listControls(): Promise<GroomControl[]>;
  /** Sets sliders (only the latest value of each is sent while an evaluation runs). */
  setParam(id: string, value: number): Promise<void>;
  setParams(values: [string, number][]): Promise<void>;
  /** Evaluates at `density` (0-1; the preview density, a share of the strands). */
  evaluate(options?: { density?: number }): Promise<GroomEvaluation>;
  /** The scalp as evaluated (it can change with the graph: a Scalp node's subdivision). */
  scalpMesh(): Promise<RootMesh>;
  /** The groom as a .groom file, with the edits made. */
  saveGroom(): Promise<Uint8Array>;
  dispose(): void;
}

/** The density evaluate uses when given none: a quick preview. */
export const DEFAULT_PREVIEW_DENSITY = 0.25;

const text = (v: string | ArrayBuffer | Uint8Array | undefined) =>
  v === undefined ? '' : typeof v === 'string' ? v : new TextDecoder().decode(v);

interface RawEvaluation {
  count: number;
  offsets: Uint32Array;
  positions: Float32Array;
  colors: Float32Array;
  guides: { offsets: Uint32Array; positions: Float32Array } | null;
  evaluateMs: number;
  totalMs: number;
  errors: string[];
  nodes: { node: string; ms: number }[];
}

/** Starts the engine's worker (`base`: where groomlab-worker.js, groomlab.js and .wasm are served). */
export function createGroomEngine(base = '/wasm/'): GroomEngine {
  const worker = new Worker(`${base}groomlab-worker.js`);
  let next = 1;
  const pending = new Map<number, { resolve: (v: unknown) => void; reject: (e: Error) => void }>();
  worker.onmessage = (e: MessageEvent<{ id: number; ok: boolean; result?: unknown; error?: string }>) => {
    const p = pending.get(e.data.id);
    if (!p) return;
    pending.delete(e.data.id);
    if (e.data.ok) p.resolve(e.data.result);
    else p.reject(new Error(e.data.error));
  };
  worker.onerror = (e) => {
    for (const p of pending.values()) p.reject(new Error(e.message || 'the GroomLab worker failed to start'));
    pending.clear();
  };
  const call = <T>(op: string, args?: unknown) =>
    new Promise<T>((resolve, reject) => {
      const id = next++;
      pending.set(id, { resolve: resolve as (v: unknown) => void, reject });
      worker.postMessage({ id, op, args });
    });

  // Slider values wait here while a set is in flight; the latest of each goes next.
  const queued = new Map<string, number>();
  let flushing: Promise<void> | null = null;
  const flush = async () => {
    while (queued.size) {
      const values = [...queued.entries()];
      queued.clear();
      await call<{ failed: string[] }>('set', { values });
    }
  };
  const setParams = (values: [string, number][]) => {
    for (const [id, v] of values) queued.set(id, v);
    if (!flushing) flushing = flush().finally(() => (flushing = null));
    return flushing;
  };

  return {
    loadGroom: (groom, mesh) => call<GroomLoaded>('load', { groom: text(groom), mesh: text(mesh) }),
    listControls: () => call<GroomControl[]>('controls'),
    setParam: (id, value) => setParams([[id, value]]),
    setParams,
    async evaluate(options) {
      if (flushing) await flushing;
      const r = await call<RawEvaluation>('evaluate', { density: options?.density ?? DEFAULT_PREVIEW_DENSITY });
      return {
        strands: { count: r.count, offsets: r.offsets, positions: r.positions, colors: r.colors },
        guides: r.guides ? { count: r.guides.offsets.length - 1, offsets: r.guides.offsets, positions: r.guides.positions } : null,
        evaluateMs: r.evaluateMs,
        totalMs: r.totalMs,
        errors: r.errors,
        nodes: r.nodes,
      };
    },
    scalpMesh: () => call<RootMesh>('scalp'),
    saveGroom: async () => new TextEncoder().encode(await call<string>('save')),
    dispose() {
      worker.terminate();
      for (const p of pending.values()) p.reject(new Error('the GroomLab engine was disposed'));
      pending.clear();
    },
  };
}

// SPDX-License-Identifier: Apache-2.0
/**
 * GroomLab against Maya 2022's XGen Interactive Grooming (Interactive Groom Splines), from the
 * reproducible benchmark in tools/bench/xgen_compare (medians of its runs). The landing page's
 * performance section and the Performance docs page both read this file: update the numbers here
 * only, from the results the harness writes (tools/bench/xgen_compare/results/).
 */

export type Count = '100k' | '500k';

/** One system's number at one strand count; absent where it does not apply. */
export interface Values {
  xgen?: number;
  groomlab?: number;
  /** GroomLab with Follow Scalp (the groom evaluated once at rest, carried by the scalp). */
  follow?: number;
  /** GroomLab without Curl: only the operators XGen runs in the benchmark. */
  nocurl?: number;
  /** GroomLab's GPU preview: a slider drag or a brush dab drawn from the GPU (the release commits on the CPU). */
  gpu?: number;
}

export interface BenchmarkRow {
  id: string;
  label: string;
  unit: 's' | 'ms' | 'MB' | 'fps';
  higherIsBetter?: boolean;
  /** Which GroomLab column the comparison uses (default: groomlab). */
  compare?: 'groomlab' | 'follow' | 'nocurl' | 'gpu';
  /** The word after the factor: "12x faster", "50x smaller". */
  better?: string;
  /** On the landing page. */
  home?: boolean;
  note?: string;
  values: Record<Count, Values>;
}

export const benchmarkInfo = {
  machine: 'AMD Threadripper PRO 5945WX (12 cores), 128 GB RAM, RTX 3090',
  host: 'Maya 2022, Windows 11',
  xgenRun: '2026-10-01',
  groomlabRun: '2026-10-02',
  groomlabVersion: '5e8bc26',
  setup:
    'The same 17k-triangle head-sized scalp, strands 15 cm long with 16 points each, clumping and noise on both (GroomLab adds a Curl), every strand shown in the viewport.',
  runs: 'Medians of 5 runs for XGen and 3 for GroomLab, each measurement in a fresh Maya.',
};

export const benchmarkRows: BenchmarkRow[] = [
  {
    id: 'create',
    label: 'New groom to first drawn frame',
    unit: 's',
    better: 'faster',
    home: true,
    values: { '100k': { xgen: 11.5, groomlab: 0.2 }, '500k': { xgen: 12.6, groomlab: 0.49 } },
  },
  {
    id: 'scene',
    label: 'Saved scene (.mb)',
    unit: 'MB',
    better: 'smaller',
    home: true,
    note: 'GroomLab saves its guides and graph; the strands are grown again on load.',
    values: { '100k': { xgen: 24.4, groomlab: 2.23 }, '500k': { xgen: 114, groomlab: 2.23 } },
  },
  {
    id: 'deform',
    label: 'Animated scalp, evaluation per frame',
    unit: 'ms',
    compare: 'follow',
    better: 'faster',
    home: true,
    note: 'Without Follow Scalp, GroomLab grows the groom again every frame.',
    values: {
      '100k': { xgen: 128, groomlab: 40.7, follow: 8.15 },
      '500k': { xgen: 414, groomlab: 166, follow: 28.0 },
    },
  },
  {
    id: 'alembic',
    label: 'Alembic export, 24 frames',
    unit: 's',
    compare: 'follow',
    better: 'faster',
    values: {
      '100k': { xgen: 4.73, groomlab: 1.32, follow: 0.59 },
      '500k': { xgen: 18.1, groomlab: 6.96, follow: 3.53 },
    },
  },
  {
    id: 'assBlur',
    label: 'Arnold .ass export, motion blur',
    unit: 's',
    compare: 'follow',
    better: 'faster',
    values: {
      '100k': { xgen: 2.47, groomlab: 0.42, follow: 0.27 },
      '500k': { xgen: 8.68, groomlab: 1.6, follow: 0.95 },
    },
  },
  {
    id: 'memory',
    label: 'Memory after creation',
    unit: 'MB',
    better: 'less',
    note: 'Process working set, batch (no viewport).',
    values: { '100k': { xgen: 286, groomlab: 138 }, '500k': { xgen: 1127, groomlab: 617 } },
  },
  {
    id: 'gpu',
    label: 'GPU memory, viewport',
    unit: 'MB',
    better: 'less',
    values: { '100k': { xgen: 190, groomlab: 99 }, '500k': { xgen: 958, groomlab: 486 } },
  },
  {
    id: 'save',
    label: 'Save the scene (.mb)',
    unit: 's',
    better: 'faster',
    values: { '100k': { xgen: 0.63, groomlab: 0.02 }, '500k': { xgen: 2.25, groomlab: 0.02 } },
  },
  {
    id: 'open',
    label: 'Open the scene and evaluate',
    unit: 's',
    better: 'faster',
    values: { '100k': { xgen: 0.47, groomlab: 0.09 }, '500k': { xgen: 1.06, groomlab: 0.22 } },
  },
  {
    id: 'clumpEval',
    label: 'Clump tweak, evaluated',
    unit: 'ms',
    better: 'faster',
    values: {
      '100k': { xgen: 51.0, groomlab: 21.9, nocurl: 15.3 },
      '500k': { xgen: 248, groomlab: 100, nocurl: 66.3 },
    },
  },
  {
    id: 'noiseEval',
    label: 'Noise tweak, evaluated',
    unit: 'ms',
    better: 'faster',
    values: {
      '100k': { xgen: 31.6, groomlab: 16.6, nocurl: 11.6 },
      '500k': { xgen: 160, groomlab: 75.7, nocurl: 49.3 },
    },
  },
  {
    id: 'noiseDraw',
    label: 'Noise tweak to redrawn frame',
    unit: 'ms',
    better: 'faster',
    values: { '100k': { xgen: 30.8, groomlab: 26.3, nocurl: 20.1 }, '500k': { xgen: 132, groomlab: 108, nocurl: 81.4 } },
  },
  {
    id: 'clumpDraw',
    label: 'Clump tweak to redrawn frame',
    unit: 'ms',
    better: 'faster',
    values: { '100k': { xgen: 80.6, groomlab: 32.0, nocurl: 24.1 }, '500k': { xgen: 367, groomlab: 134, nocurl: 98.6 } },
  },
  {
    id: 'clumpDrag',
    label: 'Dragging the clump slider, to redrawn frame',
    unit: 'ms',
    compare: 'gpu',
    better: 'faster',
    home: true,
    note: "XGen: its clump tweak to redrawn frame. GroomLab: a slider drag in the Graph Editor, previewed on the GPU; letting go commits on the CPU.",
    values: { '100k': { xgen: 80.6, gpu: 4.08 }, '500k': { xgen: 367, gpu: 13.3 } },
  },
  {
    id: 'noiseDrag',
    label: 'Dragging the noise slider, to redrawn frame',
    unit: 'ms',
    compare: 'gpu',
    better: 'faster',
    note: 'As the clump slider: XGen its noise tweak, GroomLab the drag previewed on the GPU.',
    values: { '100k': { xgen: 30.8, gpu: 3.5 }, '500k': { xgen: 132, gpu: 12.0 } },
  },
  {
    id: 'playback',
    label: 'Animated scalp playback',
    unit: 'fps',
    higherIsBetter: true,
    compare: 'follow',
    better: 'faster',
    values: {
      '100k': { xgen: 5.18, groomlab: 20.5, follow: 56.3 },
      '500k': { xgen: 1.52, groomlab: 4.92, follow: 16.4 },
    },
  },
  {
    id: 'orbit',
    label: 'Orbiting the viewport',
    unit: 'fps',
    higherIsBetter: true,
    better: 'faster',
    values: { '100k': { xgen: 557, groomlab: 280 }, '500k': { xgen: 124, groomlab: 241 } },
  },
  {
    id: 'brush',
    label: 'Brush dab while dragging, to redrawn frame',
    unit: 'ms',
    compare: 'gpu',
    note: "XGen's brushes cannot be driven by a script, so it has no number here.",
    values: { '100k': { groomlab: 13.6, nocurl: 11.9, gpu: 3.44 }, '500k': { groomlab: 36.6, nocurl: 31.6, gpu: 5.14 } },
  },
];

/** GroomLab's number a row compares with XGen's. */
export function groomlabValue(row: BenchmarkRow, count: Count): number | undefined {
  return row.values[count][row.compare ?? 'groomlab'];
}

/** How many times better GroomLab is (below 1: XGen is ahead); undefined without both numbers. */
export function factor(row: BenchmarkRow, count: Count): number | undefined {
  const x = row.values[count].xgen;
  const g = groomlabValue(row, count);
  if (x === undefined || g === undefined || x <= 0 || g <= 0) return undefined;
  return row.higherIsBetter ? g / x : x / g;
}

export function formatValue(value: number, unit: BenchmarkRow['unit']): string {
  const digits = value >= 100 ? 0 : value >= 10 ? 1 : 2;
  const text = value.toLocaleString('en-US', { maximumFractionDigits: digits, minimumFractionDigits: 0 });
  return unit === 'fps' ? `${text} fps` : `${text} ${unit}`;
}

export function formatFactor(f: number): string {
  return f >= 10 ? `${Math.round(f)}×` : `${f.toFixed(1).replace(/\.0$/, '')}×`;
}

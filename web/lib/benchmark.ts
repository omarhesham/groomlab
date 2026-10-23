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
  xgenRun: '2026-10-21',
  groomlabRun: '2026-10-23',
  groomlabVersion: '0.27.0',
  setup:
    'The same 17k-triangle head-sized scalp, strands 15 cm long with 16 points each, clumping and noise on both (GroomLab adds a Curl), every strand shown in the viewport.',
  runs: 'Medians of 3 to 5 runs, each measurement in a fresh Maya; the orbit turns about the framed head, as the tumble tool does (the GPU preview rows: 3 runs, 2026-10-02).',
};

export const benchmarkRows: BenchmarkRow[] = [
  {
    id: 'create',
    label: 'New groom to first drawn frame',
    unit: 's',
    better: 'faster',
    home: true,
    values: { '100k': { xgen: 14.6, groomlab: 0.28 }, '500k': { xgen: 13.3, groomlab: 0.57 } },
  },
  {
    id: 'scene',
    label: 'Saved scene (.mb)',
    unit: 'MB',
    better: 'smaller',
    home: true,
    note: 'GroomLab saves its guides and graph; the strands are grown again on load.',
    values: { '100k': { xgen: 24.4, groomlab: 2.24 }, '500k': { xgen: 114, groomlab: 2.24 } },
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
      '100k': { xgen: 129, groomlab: 40.4, follow: 7.81 },
      '500k': { xgen: 419, groomlab: 162, follow: 23.5 },
    },
  },
  {
    id: 'alembic',
    label: 'Alembic export, 24 frames',
    unit: 's',
    compare: 'follow',
    better: 'faster',
    values: {
      '100k': { xgen: 4.75, groomlab: 1.31, follow: 0.58 },
      '500k': { xgen: 17.7, groomlab: 6.89, follow: 3.45 },
    },
  },
  {
    id: 'assBlur',
    label: 'Arnold .ass export, motion blur',
    unit: 's',
    compare: 'follow',
    better: 'faster',
    values: {
      '100k': { xgen: 2.36, groomlab: 0.48, follow: 0.29 },
      '500k': { xgen: 8.24, groomlab: 1.82, follow: 1.01 },
    },
  },
  {
    id: 'memory',
    label: 'Memory after creation',
    unit: 'MB',
    better: 'less',
    note: 'Process working set, batch (no viewport).',
    values: { '100k': { xgen: 286, groomlab: 192 }, '500k': { xgen: 1127, groomlab: 661 } },
  },
  {
    id: 'gpu',
    label: 'GPU memory, viewport',
    unit: 'MB',
    better: 'less',
    values: { '100k': { xgen: 191, groomlab: 146 }, '500k': { xgen: 960, groomlab: 447 } },
  },
  {
    id: 'save',
    label: 'Save the scene (.mb)',
    unit: 's',
    better: 'faster',
    values: { '100k': { xgen: 0.64, groomlab: 0.02 }, '500k': { xgen: 2.07, groomlab: 0.02 } },
  },
  {
    id: 'open',
    label: 'Open the scene and evaluate',
    unit: 's',
    better: 'faster',
    values: { '100k': { xgen: 0.49, groomlab: 0.12 }, '500k': { xgen: 1.0, groomlab: 0.29 } },
  },
  {
    id: 'clumpEval',
    label: 'Clump tweak, evaluated',
    unit: 'ms',
    better: 'faster',
    values: {
      '100k': { xgen: 52.4, groomlab: 23.0, nocurl: 14.5 },
      '500k': { xgen: 252, groomlab: 103, nocurl: 60.4 },
    },
  },
  {
    id: 'noiseEval',
    label: 'Noise tweak, evaluated',
    unit: 'ms',
    better: 'faster',
    values: {
      '100k': { xgen: 33.2, groomlab: 17.0, nocurl: 11.2 },
      '500k': { xgen: 162, groomlab: 75.6, nocurl: 47.2 },
    },
  },
  {
    id: 'noiseDraw',
    label: 'Noise tweak to redrawn frame',
    unit: 'ms',
    better: 'faster',
    values: { '100k': { xgen: 36.1, groomlab: 25.6, nocurl: 21.8 }, '500k': { xgen: 132, groomlab: 91.7, nocurl: 61.2 } },
  },
  {
    id: 'clumpDraw',
    label: 'Clump tweak to redrawn frame',
    unit: 'ms',
    better: 'faster',
    values: { '100k': { xgen: 97.6, groomlab: 33.4, nocurl: 28.1 }, '500k': { xgen: 402, groomlab: 116, nocurl: 74.4 } },
  },
  {
    id: 'clumpDrag',
    label: 'Dragging the clump slider, to redrawn frame',
    unit: 'ms',
    compare: 'gpu',
    better: 'faster',
    home: true,
    note: "XGen: its clump tweak to redrawn frame. GroomLab: a slider drag in the Graph Editor, previewed on the GPU; letting go commits on the CPU.",
    values: { '100k': { xgen: 97.6, gpu: 4.08 }, '500k': { xgen: 402, gpu: 13.3 } },
  },
  {
    id: 'noiseDrag',
    label: 'Dragging the noise slider, to redrawn frame',
    unit: 'ms',
    compare: 'gpu',
    better: 'faster',
    note: 'As the clump slider: XGen its noise tweak, GroomLab the drag previewed on the GPU.',
    values: { '100k': { xgen: 36.1, gpu: 3.5 }, '500k': { xgen: 132, gpu: 12.0 } },
  },
  {
    id: 'playback',
    label: 'Animated scalp playback',
    unit: 'fps',
    higherIsBetter: true,
    compare: 'follow',
    better: 'faster',
    values: {
      '100k': { xgen: 4.78, groomlab: 20.1, follow: 92.1 },
      '500k': { xgen: 1.46, groomlab: 5.61, follow: 94.6 },
    },
  },
  {
    id: 'orbit',
    label: 'Orbiting the viewport',
    unit: 'fps',
    higherIsBetter: true,
    better: 'faster',
    values: { '100k': { xgen: 491, groomlab: 525 }, '500k': { xgen: 116, groomlab: 184 } },
  },
  {
    id: 'brush',
    label: 'Brush dab while dragging, to redrawn frame',
    unit: 'ms',
    compare: 'gpu',
    note: "XGen's brushes cannot be driven by a script, so it has no number here.",
    values: { '100k': { groomlab: 19.2, nocurl: 15.9, gpu: 3.44 }, '500k': { groomlab: 57.8, nocurl: 41.9, gpu: 5.14 } },
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

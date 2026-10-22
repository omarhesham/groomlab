// SPDX-License-Identifier: Apache-2.0
/*
 * The home page's real grooms: .groom files made by the GroomLab engine from its presets on the Manny
 * head (engine/web/tools/make_home_samples.mjs, CC BY 4.0), evaluated in the browser by the engine
 * built to WebAssembly (home-engine.ts, loaded lazily). Small and dependency free, so the page can
 * draw its controls before the engine has loaded: the values here mirror what the generator printed
 * (the engine's own ranges, labels and values replace them once it answers).
 */

/** A slider: one of the groom's exposed params, as the engine lists it (GroomControl's fields). */
export interface EngineSlider {
  /** "param:<node>.<param>": what setParam takes. */
  id: string;
  label: string;
  min: number;
  max: number;
  step: number;
  value: number;
}

/** What a viewer asks of the engine: a groom, and how many strands it shows. */
export interface EngineSource {
  /** The .groom's URL. */
  url: string;
  /** Its slot in the page's one engine (one groom per slot). */
  slot: string;
  /** The groom's strands at full density (the generator's count): the preview density is a share of it. */
  fullStrands: number;
  /** About how many strands the viewer draws (desktop; phones take ~half). */
  strands: number;
  /** A param scaling the strand count (Scatter's density) and its value in the file: the count follows it, up to `maxStrands`. */
  densityParam?: [string, number];
  /** The most strands the viewer draws whatever that param says. */
  maxStrands?: number;
}

/** What the page sets on a groom: param values ("param:<node>.<param>") and bypassed nodes (by name). */
export interface EngineEdits {
  params?: Record<string, number>;
  bypass?: Record<string, boolean>;
}

/** The hero's big head of curls: Curly, tuned. Its chip reads Curl's radius and the length scale. */
export const HERO_GROOM: EngineSource = {
  url: '/samples/manny_curls.groom',
  slot: 'hero',
  fullStrands: 79040,
  strands: 11000,
};
/** Short curls on the Try section's chain, node for node. */
export const SHORT_CURLS_URL = '/samples/manny_short_curls.groom';
export const SHORT_CURLS_FULL = 85908;
/** Scatter's seed in the file (a new seed: new roots). */
export const SHORT_CURLS_SEED = 3;

/** The groom's exposed params, in the Try section's order. */
export const SHORT_CURLS_SLIDERS: EngineSlider[] = [
  { id: 'param:scatter.density', label: 'Density', min: 0, max: 2000, step: 0.1, value: 140 },
  { id: 'param:clump.strength', label: 'Strength', min: -1, max: 1, step: 0.001, value: 0.7 },
  { id: 'param:clump.count', label: 'Count', min: 0, max: 1000, step: 1, value: 220 },
  { id: 'param:curl.radius', label: 'Radius', min: 0, max: 2, step: 0.001, value: 0.4 },
  { id: 'param:curl.frequency', label: 'Frequency', min: 0, max: 2, step: 0.001, value: 0.5 },
  { id: 'param:noise.amplitude', label: 'Amplitude', min: 0, max: 1, step: 0.001, value: 0.08 },
  { id: 'param:color.melanin', label: 'Melanin', min: 0, max: 1, step: 0.001, value: 0.55 },
  { id: 'param:color.redness', label: 'Redness', min: 0, max: 1, step: 0.001, value: 0.45 },
];

export const sliderValues = (sliders: EngineSlider[]) => Object.fromEntries(sliders.map((s) => [s.id, s.value]));

// SPDX-License-Identifier: Apache-2.0
// Compares two raw float32 position dumps (compare.mjs --dump, groomlab_web_probe --dump): how many
// floats differ in their bits, the largest difference, and how far that is in units in the last
// place (ULPs) of the values.
//
//   node engine/web/tools/diff_positions.mjs native.f32 wasm.f32

import { readFileSync } from 'node:fs';

const [a, b] = process.argv.slice(2).map((p) => {
  const bytes = readFileSync(p);
  return new Float32Array(bytes.buffer, bytes.byteOffset, bytes.byteLength / 4);
});
if (!a || !b) {
  console.error('usage: node diff_positions.mjs a.f32 b.f32');
  process.exit(2);
}
if (a.length !== b.length) {
  console.log(`different lengths: ${a.length} and ${b.length} floats`);
  process.exit(1);
}
const ai = new Int32Array(a.buffer, a.byteOffset, a.length);
const bi = new Int32Array(b.buffer, b.byteOffset, b.length);
let differ = 0;
let maxAbs = 0;
let maxUlp = 0;
let worst = -1;
for (let i = 0; i < a.length; i++) {
  if (ai[i] === bi[i]) continue;
  differ++;
  const d = Math.abs(a[i] - b[i]);
  const ulp = Math.abs(ai[i] - bi[i]);
  if (d > maxAbs) {
    maxAbs = d;
    worst = i;
  }
  maxUlp = Math.max(maxUlp, ulp);
}
const points = a.length / 3;
console.log(
  `${a.length} floats (${points} points): ${differ} differ (${((100 * differ) / a.length).toFixed(3)}%), ` +
    `largest difference ${maxAbs.toExponential(3)}` +
    (worst >= 0 ? ` at point ${Math.floor(worst / 3)} (${a[worst]} vs ${b[worst]})` : '') +
    `, up to ${maxUlp} ULP`,
);

// SPDX-License-Identifier: Apache-2.0
// The WebAssembly module (web/public/wasm/groomlab.js) evaluating a groom in Node, printing what
// groomlab_web_probe (engine/web/src/web_probe.cpp, the same binding built natively) prints, so
// the two builds' strands can be compared line by line (docs/WEB.md, "Checking it"):
//
//   node engine/web/tools/compare.mjs <file.groom | preset.json mesh.obj> [--density 0.25] [--set id=value]... [--dump f]
//   groomlab_web_probe                <file.groom | preset.json mesh.obj> [--density 0.25] [--set id=value]... [--dump f]
//
// --dump writes the positions as raw float32 (engine/web/tools/diff_positions.mjs compares two).

import { readFileSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const require = createRequire(import.meta.url);
const createGroomLab = require(resolve(here, '../../../web/public/wasm/groomlab.js'));

const files = [];
const sets = [];
let density = 0.25;
let repeat = 1;
let dump = '';
const args = process.argv.slice(2);
for (let i = 0; i < args.length; i++) {
  if (args[i] === '--density') density = Number(args[++i]);
  else if (args[i] === '--set') sets.push(args[++i].split('='));
  else if (args[i] === '--repeat') repeat = Number(args[++i]);
  else if (args[i] === '--dump') dump = args[++i];
  else files.push(args[i]);
}
if (!files.length) {
  console.error('usage: node compare.mjs <file.groom | preset.json mesh.obj> [--density d] [--set id=value] [--repeat n]');
  process.exit(2);
}

// FNV-1a over the floats' bytes (web_probe hashes the same).
function hashFloats(f32) {
  const bytes = new Uint8Array(f32.buffer, f32.byteOffset, f32.byteLength);
  let h = 0xcbf29ce484222325n;
  for (const b of bytes) {
    h ^= BigInt(b);
    h = (h * 0x100000001b3n) & 0xffffffffffffffffn;
  }
  return h.toString(16);
}

const fmt = (x) => Number(x.toPrecision(9)).toString();

const M = await createGroomLab();
const groom = new M.WebGroom();
let t0 = performance.now();
if (!groom.load(readFileSync(files[0], 'utf8'), files[1] ? readFileSync(files[1], 'utf8') : '')) {
  console.error('error: ' + groom.error());
  process.exit(1);
}
const loadMs = performance.now() - t0;
for (const [id, value] of sets) if (!groom.setParam(id, Number(value))) console.error('warning: cannot set ' + id);
const times = [];
for (let r = 0; r < repeat; r++) {
  if (!groom.evaluate(density)) {
    console.error('error: ' + groom.error());
    process.exit(1);
  }
  times.push(groom.evaluateMs());
}
const off = groom.offsets().slice();
const pos = groom.positions().slice();
const n = groom.strandCount();
if (dump) writeFileSync(dump, new Uint8Array(pos.buffer));
console.log(`strands ${n}\npoints ${pos.length / 3}\nhash ${hashFloats(pos)}`);
for (const s of [0, n >> 1, n ? n - 1 : 0]) {
  if (s >= n) continue;
  const a = off[s] * 3;
  const b = (off[s + 1] - 1) * 3;
  console.log(`strand ${s} root ${fmt(pos[a])} ${fmt(pos[a + 1])} ${fmt(pos[a + 2])} tip ${fmt(pos[b])} ${fmt(pos[b + 1])} ${fmt(pos[b + 2])}`);
}
console.error(`load ${loadMs.toFixed(1)} ms, evaluate ${times.map((t) => t.toFixed(1)).join(' / ')} ms`);
groom.delete();

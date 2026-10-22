// SPDX-License-Identifier: Apache-2.0
// Writes the website's sample groom (web/public/samples/manny_hair.groom) with the WebAssembly
// module (build it first: docs/WEB.md): the Hair basics preset on the Manny head
// (hosts/maya/meshes/manny_head.obj, CC BY 4.0), its strands kept to the hair's part of the head by
// an expression, its guides baked from a small combing graph (as tests/hosts/maya/gui_presets.py
// grooms the preset sheets' heads), and a Control node, "frizz", scaling the noise.
//
//   node engine/web/tools/make_sample.mjs

import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const require = createRequire(import.meta.url);
const createGroomLab = require(resolve(root, 'web/public/wasm/groomlab.js'));

// Where hair grows on Manny (cm, y up, facing +z): above a hairline low at the nape and over the
// ears, higher at the temples and the brow. 0 to 1 at the root.
const HAIR = [
  '$front = smoothstep($z, -3, 7);',
  '$line = -3.5 + 12.0 * $front + 2.5 * smoothstep(abs($x), 6, 9) * (1 - $front);',
  'smoothstep($y, $line, $line + 1.5)',
].join(' ');

const LENGTH = 24; // the back's guide length, cm
const SPACING = 2.4; // between guides, cm

// Guides as an artist grooms a shoulder-length style: longer at the back, combed away from a center
// part and back off the face, a little lift, then hanging under gravity and kept off the head.
function combGraph() {
  const side = '(smoothstep($nx, -0.12, 0.12) * 2 - 1)';
  const nodes = [
    { name: 'scalp', type: 'Scalp' },
    {
      name: 'roots',
      type: 'Scatter',
      params: { density: { expression: HAIR, min: 0, max: 1 / (SPACING * SPACING) }, length: LENGTH, randomness: 0, seed: 3 },
    },
    {
      name: 'lengths',
      type: 'Transform',
      params: { scaleRandom: 0.06, scale: { expression: '1 - 0.72 * smoothstep($nz, -0.2, 0.75)', min: 0, max: 1 } },
    },
    {
      name: 'part',
      type: 'Bend',
      params: {
        direction: {
          expression: `normalize([${side} * (1 + 2 * smoothstep($nz, 0.3, 0.9)), -0.5 + 1.2 * smoothstep($nz, 0.35, 0.9), -0.6 - 1.4 * smoothstep($nz, 0.3, 0.85)])`,
        },
        angle: 60,
        start: 0,
        end: 0.2,
        along: 'direction',
      },
    },
    { name: 'lift', type: 'Bend', params: { angle: 8, end: 0 } },
    { name: 'hang', type: 'Gravity', params: { strength: 1.3, stiffness: 0.2 } },
    { name: 'collide', type: 'Collide', params: { offset: LENGTH * 0.012, maxDepth: LENGTH, iterations: 4 } },
    { name: 'resample', type: 'Resample', params: { segments: 12 } },
    { name: 'output', type: 'Output' },
  ];
  const chain = ['roots', 'lengths', 'part', 'lift', 'hang', 'collide', 'resample', 'output'];
  const connections = [
    { from: 'scalp', to: 'roots', input: 'scalp' },
    { from: 'scalp', to: 'collide', input: 'scalp' },
    ...chain.slice(1).map((to, i) => ({ from: chain[i], to, input: 'strands' })),
  ];
  return { version: 1, output: 'output', nodes, connections };
}

const M = await createGroomLab();
const groom = new M.WebGroom();
const preset = readFileSync(resolve(root, 'hosts/maya/presets/hair_basics.json'), 'utf8');
const mesh = readFileSync(resolve(root, 'hosts/maya/meshes/manny_head.obj'), 'utf8');
const check = (ok, what) => {
  if (!ok) {
    console.error(`${what}: ${groom.error()}`);
    process.exit(1);
  }
};
check(groom.load(preset, mesh), 'load');
check(groom.bakeGuides(JSON.stringify(combGraph())), 'guides');

// The preset's strands on the hair's part of the head only, kept off it like the guides, and a
// Control node, "frizz", scaling Noise's amplitude (plugged into its exposed amplitude).
const graph = JSON.parse(groom.graphJson());
const node = (name) => graph.nodes.find((n) => n.name === name);
const scatter = node('scatter');
scatter.params.density = { expression: HAIR, min: 0, max: scatter.params.density * 1.6 };
const noise = node('noise');
noise.paramInputs = ['amplitude'];
noise.params.amplitude *= 2;
graph.nodes.push({ name: 'frizz', type: 'Control', params: { name: 'frizz', default: 1, min: 0, max: 4 } });
graph.connections.push({ from: 'frizz', to: 'noise', input: 'amplitude' });
check(groom.setGraphJson(JSON.stringify(graph)), 'graph');

check(groom.evaluate(1), 'evaluate');
const stats = JSON.parse(groom.statsJson());
if (stats.errors.length) console.error(stats.errors.join('\n'));
const out = resolve(root, 'web/public/samples/manny_hair.groom');
mkdirSync(dirname(out), { recursive: true });
writeFileSync(out, groom.saveGroom());
console.log(`${out}: ${groom.strandCount()} strands at full density, ${stats.points} points, ${groom.evaluateMs().toFixed(0)} ms`);
console.log(JSON.parse(groom.controlsJson()).map((c) => `${c.id} ${c.min}..${c.max} = ${c.value}`).join('\n'));
groom.delete();

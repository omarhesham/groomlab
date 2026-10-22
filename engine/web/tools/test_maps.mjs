// SPDX-License-Identifier: Apache-2.0
// Checks the WebAssembly module's maps (docs/WEB.md, "Maps"): painted vertex maps (inside the
// .groom), texture maps handed in with WebGroom.addFile (at the groom's paths, by plain name, UDIM
// tile sets, paths with ".."), missingFilesJson, and re-evaluation once a file arrives or changes.
// Uses the sample groom (web/public/samples: its "cut" vertex map and "highlights" texture map) and
// tests/data/compat (a scatter density read from ../../textures/density.pgm). Exits 1 on a failure.
//
//   node engine/web/tools/test_maps.mjs

import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { grayPng } from './png.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const require = createRequire(import.meta.url);
const createGroomLab = require(resolve(root, 'web/public/wasm/groomlab.js'));
const M = await createGroomLab();

let failures = 0;
const expect = (ok, what) => {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${what}`);
  if (!ok) failures++;
};

function hash(f32) {
  const bytes = new Uint8Array(f32.buffer, f32.byteOffset, f32.byteLength);
  let h = 0xcbf29ce484222325n;
  for (const b of bytes) h = ((h ^ BigInt(b)) * 0x100000001b3n) & 0xffffffffffffffffn;
  return h.toString(16);
}

/** Loads `text`, adds `files` ([path, bytes]), evaluates: {groom, ok, positions, colors, missing}. */
function run(text, files = [], density = 0.25) {
  const groom = new M.WebGroom();
  if (!groom.load(text, '')) throw new Error(groom.error());
  for (const [path, bytes] of files) if (!groom.addFile(path, bytes)) throw new Error(groom.error());
  const missing = JSON.parse(groom.missingFilesJson());
  const ok = groom.evaluate(density);
  const result = { groom, ok, missing, error: groom.error() };
  if (ok) {
    result.positions = hash(groom.positions().slice());
    result.colors = hash(groom.colors().slice());
  }
  return result;
}
const done = (r) => {
  r.groom.clearFiles();
  r.groom.delete();
};

const sampleText = readFileSync(resolve(root, 'web/public/samples/manny_hair.groom'), 'utf8');
const texture = 'masks/manny_hair/highlights.png';
const png = readFileSync(resolve(root, 'web/public/samples', texture));

// 1. The texture map at its path: found, evaluated.
const full = run(sampleText, [[texture, png]]);
expect(full.ok && full.missing.length === 0, `sample with ${texture}: evaluates, nothing missing`);
done(full);

// 2. Nothing given: the texture is missing, and said to be.
const none = run(sampleText);
expect(JSON.stringify(none.missing) === JSON.stringify([texture]), `sample alone: missing ${JSON.stringify(none.missing)}`);
expect(!none.ok && /highlights\.png/.test(none.error), `sample alone: the evaluation names the file (${none.error})`);
// ...then given by plain name in the same session: found where the groom looks, and evaluated again.
expect(none.groom.addFile('highlights.png', png) && none.groom.missingFilesJson() === '[]', 'added by name: found');
expect(none.groom.evaluate(0.25) && hash(none.groom.colors().slice()) === full.colors, 'added by name: the same strands as at its path');
// ...then rewritten (all black, no highlights): read again, other colors.
const black = grayPng(16, 16, new Uint8Array(256));
none.groom.addFile(texture, black);
expect(none.groom.evaluate(0.25) && hash(none.groom.colors().slice()) !== full.colors, 'texture rewritten: re-read (colors change)');
done(none);

// 3. The texture map matters: a black one gives other colors, the same positions.
const dark = run(sampleText, [[texture, black]]);
expect(dark.ok && dark.colors !== full.colors && dark.positions === full.positions, 'black texture: other colors, same positions');
done(dark);

// 4. The vertex map matters: the cut kept whole (every vertex 1) gives other positions.
const file = JSON.parse(sampleText);
const whole = structuredClone(file);
whole.scalp.colorSets.cut = whole.scalp.colorSets.cut.map(() => 1);
const uncut = run(JSON.stringify(whole), [[texture, png]]);
expect(uncut.ok && uncut.positions !== full.positions, 'vertex map "cut" all 1: other positions (the map is read)');
done(uncut);
// ...and its values evaluated as stored: halving the map shortens the strands further.
const half = structuredClone(file);
half.scalp.colorSets.cut = half.scalp.colorSets.cut.map((v, i) => (i % 4 === 3 ? 1 : v * 0.5));
const shorter = run(JSON.stringify(half), [[texture, png]]);
expect(shorter.ok && shorter.positions !== full.positions && shorter.positions !== uncut.positions, 'vertex map halved: other positions again');
done(shorter);

// 5. A UDIM tile set: <UDIM> in the stored path, tile 1001 given by name (UVs 0-1 read tile 1001).
const udim = structuredClone(file);
udim.maps.highlights.texture = 'masks/manny_hair/highlights.<UDIM>.png';
const tiles = run(JSON.stringify(udim));
expect(JSON.stringify(tiles.missing) === '["masks/manny_hair/highlights.<UDIM>.png"]', 'UDIM set alone: missing');
tiles.groom.addFile('highlights.1001.png', png);
tiles.groom.addFile('HIGHLIGHTS.1002.PNG', black);
const udimFiles = JSON.parse(tiles.groom.filesJson());
expect(tiles.groom.missingFilesJson() === '[]' && udimFiles[0].udim, 'UDIM tiles added by name (any case): found');
expect(tiles.groom.evaluate(0.25) && hash(tiles.groom.colors().slice()) === full.colors, 'UDIM tile 1001: the same strands as the plain texture');
done(tiles);

// 6. A path with "..": the compat groom's density texture, from a folder tree and by name.
const compatText = readFileSync(resolve(root, 'tests/data/compat/v1/groom.groom'), 'utf8');
const pgm = readFileSync(resolve(root, 'tests/data/textures/density.pgm'));
const tree = run(compatText, [['../../textures/density.pgm', pgm]], 1);
const byName = run(compatText, [['density.pgm', pgm]], 1);
expect(tree.ok && tree.missing.length === 0, `compat with ../../textures/density.pgm: ${tree.ok ? 'evaluates' : tree.error}`);
expect(byName.ok && byName.positions === tree.positions, 'compat with density.pgm by name: the same strands');
done(tree);
done(byName);

console.log(failures ? `${failures} failed` : 'all passed');
process.exit(failures ? 1 : 0);

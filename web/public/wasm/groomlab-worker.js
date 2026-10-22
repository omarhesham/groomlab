// SPDX-License-Identifier: Apache-2.0
/*
 * The GroomLab engine in a worker: groomlab.js and groomlab.wasm (engine/web, built with the wasm
 * preset: docs/WEB.md) evaluating a groom off the page's thread. web/lib/groomlab-wasm is its client.
 * Messages: {id, op, args} in, {id, ok, result} or {id, ok: false, error} out; buffers are copied
 * out of the module's memory and transferred.
 */
/* global importScripts, createGroomLab */
'use strict';

let modulePromise = null;
let groom = null;

function engine() {
  if (!modulePromise) {
    importScripts('groomlab.js');
    modulePromise = createGroomLab();
  }
  return modulePromise;
}

function need() {
  if (!groom) throw new Error('no groom loaded');
  return groom;
}

function scalpOf(g) {
  return {
    positions: g.scalpPositions().slice(),
    normals: g.scalpNormals().slice(),
    indices: g.scalpIndices().slice(),
  };
}

const ops = {
  async load({ groom: text, mesh }) {
    const M = await engine();
    const next = new M.WebGroom();
    const t0 = performance.now();
    if (!next.load(text, mesh || '')) {
      const error = next.error();
      next.delete();
      throw new Error(error);
    }
    if (groom) groom.delete();
    groom = next;
    return { version: M.version(), loadMs: performance.now() - t0, controls: JSON.parse(groom.controlsJson()), scalp: scalpOf(groom) };
  },
  controls() {
    return JSON.parse(need().controlsJson());
  },
  set({ values }) {
    const g = need();
    const failed = [];
    for (const [id, value] of values) if (!g.setParam(id, value)) failed.push(id + (g.error() ? ': ' + g.error() : ''));
    return { failed };
  },
  evaluate({ density }) {
    const g = need();
    const t0 = performance.now();
    const ok = g.evaluate(density);
    const stats = JSON.parse(g.statsJson());
    if (!ok) throw new Error(g.error());
    const guideOffsets = g.guideOffsets().slice();
    return {
      count: g.strandCount(),
      offsets: g.offsets().slice(),
      positions: g.positions().slice(),
      colors: g.colors().slice(),
      guides: guideOffsets.length > 1 ? { offsets: guideOffsets, positions: g.guidePositions().slice() } : null,
      evaluateMs: g.evaluateMs(),
      totalMs: performance.now() - t0,
      errors: stats.errors,
      nodes: stats.nodes,
    };
  },
  scalp() {
    return scalpOf(need());
  },
  save() {
    return need().saveGroom();
  },
};

function transferables(result) {
  const out = [];
  const walk = (v) => {
    if (!v || typeof v !== 'object') return;
    if (ArrayBuffer.isView(v)) out.push(v.buffer);
    else if (!Array.isArray(v)) Object.values(v).forEach(walk);
  };
  walk(result);
  return out;
}

self.onmessage = async (e) => {
  const { id, op, args } = e.data;
  try {
    if (!ops[op]) throw new Error('unknown op ' + op);
    const result = await ops[op](args || {});
    self.postMessage({ id, ok: true, result }, transferables(result));
  } catch (err) {
    self.postMessage({ id, ok: false, error: String((err && err.message) || err) });
  }
};

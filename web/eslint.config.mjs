// SPDX-License-Identifier: Apache-2.0
import { defineConfig, globalIgnores } from 'eslint/config';
import nextVitals from 'eslint-config-next/core-web-vitals';

const eslintConfig = defineConfig([
  ...nextVitals,
  globalIgnores([
    '.next/**',
    'out/**',
    'build/**',
    'next-env.d.ts',
    '.source/**',
    // Emscripten's output (engine/web, docs/WEB.md): generated, not ours to lint.
    'public/wasm/groomlab.js',
  ]),
]);

export default eslintConfig;

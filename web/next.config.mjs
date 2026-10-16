// SPDX-License-Identifier: Apache-2.0
import { createMDX } from 'fumadocs-mdx/next';

const withMDX = createMDX();

/** @type {import('next').NextConfig} */
const config = {
  // Static export: `npm run build` writes a plain site to out/ that any host can serve.
  output: 'export',
  // No image server in a static export; images are served as they are.
  images: { unoptimized: true },
  reactStrictMode: true,
};

export default withMDX(config);

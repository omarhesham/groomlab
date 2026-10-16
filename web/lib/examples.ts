// SPDX-License-Identifier: Apache-2.0
import fs from 'node:fs';
import path from 'node:path';

/**
 * Example images live in public/examples and are produced from Maya separately,
 * so any of them may be missing. These helpers run at build time (server
 * components only) so the site can fall back gracefully.
 */
const examplesDir = path.join(process.cwd(), 'public', 'examples');

export interface ExampleInfo {
  src: string;
  width: number;
  height: number;
}

/** The image's public URL and pixel size, or null when the file is missing. */
export function getExample(name: string): ExampleInfo | null {
  const file = path.join(examplesDir, name);
  let fd: number | undefined;
  try {
    fd = fs.openSync(file, 'r');
    const header = Buffer.alloc(24);
    fs.readSync(fd, header, 0, 24, 0);
    const isPng = header.readUInt32BE(0) === 0x89504e47;
    const width = isPng ? header.readUInt32BE(16) : 1600;
    const height = isPng ? header.readUInt32BE(20) : 900;
    return { src: `/examples/${name}`, width, height };
  } catch {
    return null;
  } finally {
    if (fd !== undefined) fs.closeSync(fd);
  }
}

export function hasExample(name: string): boolean {
  return getExample(name) !== null;
}

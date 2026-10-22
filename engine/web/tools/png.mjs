// SPDX-License-Identifier: Apache-2.0
// An 8-bit gray PNG writer for the web tools (make_sample.mjs's texture map, test_maps.mjs's).

import { crc32, deflateSync } from 'node:zlib';

/** An 8-bit gray PNG of `pixels` (width x height bytes, rows top-down). */
export function grayPng(width, height, pixels) {
  const chunk = (type, data) => {
    const out = Buffer.alloc(12 + data.length);
    out.writeUInt32BE(data.length, 0);
    out.write(type, 4, 'latin1');
    data.copy(out, 8);
    out.writeUInt32BE(crc32(out.subarray(4, 8 + data.length)), 8 + data.length);
    return out;
  };
  const header = Buffer.alloc(13);
  header.writeUInt32BE(width, 0);
  header.writeUInt32BE(height, 4);
  header.set([8, 0, 0, 0, 0], 8); // 8 bits, gray, deflate, filter method 0, no interlace
  const rows = Buffer.alloc((width + 1) * height); // each row: filter type 0, then its bytes
  for (let y = 0; y < height; y++) rows.set(pixels.subarray(y * width, (y + 1) * width), y * (width + 1) + 1);
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', header),
    chunk('IDAT', deflateSync(rows, { level: 9 })),
    chunk('IEND', Buffer.alloc(0)),
  ]);
}

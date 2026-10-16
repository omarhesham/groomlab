// SPDX-License-Identifier: Apache-2.0
import type { CSSProperties } from 'react';
import { cn } from '@/lib/cn';

/*
 * The site's hair motif: SVG strands with pathLength="1", so drawing one is
 * just its dash offset (see `.strand`, `.draw-reveal` and `.draw-scroll` in
 * global.css). All of them are decoration and hidden from screen readers.
 */

/** A single curl, the logo's tallest strand. Draws itself when its section is revealed. */
export function Curl({ color, className, delay = 0.25 }: { color: string; className?: string; delay?: number }) {
  return (
    // Drawn 3 units high in its box, so the strand's root sits on the label's baseline (as the logo's do).
    <svg aria-hidden="true" viewBox="0 3 24 24" fill="none" className={cn('shrink-0 overflow-visible', className)}>
      <path
        d="M6.4 21 C6.4 14.25 7.9 8.6 13.1 5.6 C16.9 3.6 20.4 5.7 19.35 8.85 C18.5 11.25 15.15 10.95 15.45 8.55"
        pathLength={1}
        className="strand draw-reveal"
        stroke={color}
        strokeWidth="2.8"
        style={{ '--d': `${delay}s` } as CSSProperties}
      />
    </svg>
  );
}

/*
 * A lock of hair: five strands that leave the left edge bundled together,
 * ride one wave, then fan out on the right, the tallest ending in the logo's curl.
 */
const FLOW = [
  {
    color: '#2F7BF6',
    d: 'M-40 132 C 240 44, 470 46, 700 112 S 1010 150, 1150 92 C 1230 58, 1330 50, 1352 96 C 1368 132, 1318 146, 1300 116',
  },
  { color: '#FFC53D', d: 'M-40 138 C 240 52, 470 54, 700 120 S 1040 168, 1220 138 S 1400 116, 1480 118' },
  { color: '#2ECC8F', d: 'M-40 144 C 240 60, 470 62, 700 128 S 1060 186, 1480 170' },
  { color: '#FF4D4F', d: 'M-40 150 C 240 68, 470 70, 700 136 S 1030 198, 1210 206 S 1400 222, 1480 236' },
  { color: '#A36BFF', d: 'M-40 156 C 240 76, 470 78, 700 144 S 960 196, 1060 208 C 1110 214, 1150 200, 1146 184' },
];

/**
 * A lock of five strands in the primaries, flowing across the page. It grows
 * as it scrolls into view (or, with `scroll={false}` or where scroll timelines
 * are missing, when revealed) and then drifts gently.
 *
 * It scales evenly with the page width, so the curls are never cropped
 * (`.flow-strand` in global.css keeps the lines about 3px thick at any size).
 * The scale must stay uniform: the draw is a dash in pathLength units, which
 * a stretched or non-scaling stroke would cut short.
 */
export function StrandFlow({
  className,
  range,
  scroll = true,
}: {
  className?: string;
  range?: string;
  scroll?: boolean;
}) {
  return (
    <svg
      aria-hidden="true"
      viewBox="0 0 1440 248"
      fill="none"
      className={cn('draw-scope block aspect-[1440/248] h-auto w-full', className)}
    >
      {FLOW.map((s, i) => (
        <g
          key={s.color}
          className="drift"
          style={{ '--drift-dur': `${7 + i * 0.8}s`, '--drift-delay': `${-i * 1.3}s` } as CSSProperties}
        >
          <path
            d={s.d}
            pathLength={1}
            className={cn('strand flow-strand draw-reveal', scroll && 'draw-scroll')}
            stroke={s.color}
            style={{ '--d': `${0.1 + i * 0.12}s`, ...(range ? { '--draw-range': range } : {}) } as CSSProperties}
          />
        </g>
      ))}
    </svg>
  );
}

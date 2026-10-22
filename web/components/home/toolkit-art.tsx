// SPDX-License-Identifier: Apache-2.0
import type { CSSProperties } from 'react';
import { category } from './graph-theme';

/*
 * The toolkit cards' little scenes, drawn in SVG and moved by CSS (`.art-*` in global.css): a brush
 * combing guides, a map painted on the scalp, a tube lock filling with strands, strands becoming
 * cards, hair swinging on its roots, a feather and scales. All decoration (aria-hidden); they play
 * once their card is on screen and hold still with reduced motion.
 */

const r1 = (n: number) => Math.round(n * 10) / 10;
const VB = '0 0 320 200';
const style = (vars: Record<string, string | number>) => vars as CSSProperties;

/** The scalp: a shallow arc along the bottom, and the y of its surface at x. */
const scalpY = (x: number) => 176 - 18 * Math.sin((Math.PI * (x - 10)) / 300);
const SCALP = `M10 ${r1(scalpY(10))} Q160 ${r1(176 - 36)} 310 ${r1(scalpY(310))}`;

function Scalp() {
  return (
    <>
      <path d={`${SCALP} L310 200 L10 200 Z`} fill="#1b1d23" />
      <path d={SCALP} fill="none" stroke="#33363e" strokeWidth="1.5" />
    </>
  );
}

/* ------------------------------------------------------------------ brush */
const brushGuides = Array.from({ length: 9 }, (_, i) => {
  const x = 46 + i * 29;
  const y = scalpY(x);
  const h = 92 + ((i * 37) % 23);
  return { x, y, d: `M${x} ${r1(y)} C${x} ${r1(y - h * 0.45)} ${x + 4} ${r1(y - h * 0.8)} ${x + 26} ${r1(y - h)}`, h };
});

export function BrushArt() {
  const c = category.Shape;
  return (
    <svg aria-hidden="true" viewBox={VB} className="art size-full" fill="none">
      {brushGuides.map((g, i) => (
        <g
          key={i}
          className="art-comb"
          style={style({ transformOrigin: `${g.x}px ${r1(g.y)}px`, animationDelay: `${(0.02 + i * 0.062) * 3.6}s` })}
        >
          {[-9, 9].map((dx) => (
            <path
              key={dx}
              d={`M${g.x + dx} ${r1(scalpY(g.x + dx))} C${g.x + dx} ${r1(g.y - g.h * 0.4)} ${g.x + dx + 4} ${r1(g.y - g.h * 0.72)} ${g.x + dx + 22} ${r1(g.y - g.h * 0.9)}`}
              stroke="#474b54"
              strokeWidth="1.2"
              strokeLinecap="round"
            />
          ))}
          <path d={g.d} stroke={c.ink} strokeWidth="2.2" strokeLinecap="round" />
          {[0.33, 0.66, 1].map((t) => {
            const x = g.x + (t === 1 ? 26 : t === 0.66 ? 6 : 1);
            const y = g.y - g.h * (t === 1 ? 1 : t === 0.66 ? 0.66 : 0.33);
            return <circle key={t} cx={r1(x)} cy={r1(y)} r="2.6" fill="#0b0c0f" stroke={c.ink} strokeWidth="1.5" />;
          })}
        </g>
      ))}
      <Scalp />
      <g className="art-ring">
        <circle cx="30" cy="88" r="27" stroke={c.ink} strokeWidth="1.5" strokeDasharray="4 4" fill={c.tint} />
        <circle cx="30" cy="88" r="2.5" fill={c.ink} />
      </g>
    </svg>
  );
}

/* ------------------------------------------------------------------ paint */
const paintStrands = Array.from({ length: 19 }, (_, i) => {
  const x = 22 + i * 15;
  const y = scalpY(x);
  // The painted map: long in the middle, short at the edges.
  const v = Math.sin((Math.PI * (x - 10)) / 300) ** 1.6;
  const h = 26 + 96 * v;
  return { x, y, h, lean: (x - 160) * 0.08 };
});

export function PaintArt() {
  const c = category.Fields;
  return (
    <svg aria-hidden="true" viewBox={VB} className="art size-full" fill="none">
      <defs>
        <linearGradient id="paint-map" x1="10" x2="310" y1="0" y2="0" gradientUnits="userSpaceOnUse">
          <stop offset="0" stopColor={c.hue} stopOpacity="0.15" />
          <stop offset="0.5" stopColor={c.ink} />
          <stop offset="1" stopColor={c.hue} stopOpacity="0.15" />
        </linearGradient>
      </defs>
      {paintStrands.map((s, i) => (
        <path
          key={i}
          className="art-grow"
          style={style({ transformOrigin: `${s.x}px ${r1(s.y)}px`, animationDelay: `${(i / 18) * 0.6 * 4.8}s` })}
          d={`M${s.x} ${r1(s.y)} C${s.x} ${r1(s.y - s.h * 0.5)} ${r1(s.x + s.lean * 0.4)} ${r1(s.y - s.h * 0.8)} ${r1(s.x + s.lean + 8)} ${r1(s.y - s.h)}`}
          stroke="#9a9ea7"
          strokeWidth="1.6"
          strokeLinecap="round"
        />
      ))}
      <Scalp />
      <path d={SCALP} className="art-paint" stroke="url(#paint-map)" strokeWidth="9" strokeLinecap="round" pathLength={1} />
      <g className="art-brush">
        <circle cx="10" cy={r1(scalpY(10))} r="15" stroke={c.ink} strokeWidth="1.5" fill="#0b0c0f" fillOpacity="0.6" />
        <circle cx="10" cy={r1(scalpY(10))} r="4" fill={c.ink} />
      </g>
    </svg>
  );
}

/* ------------------------------------------------------------------ tubes */
function bez(t: number, p: number[][]) {
  const u = 1 - t;
  const b = [u * u * u, 3 * u * u * t, 3 * u * t * t, t * t * t];
  return [0, 1].map((k) => b.reduce((s, w, i) => s + w * p[i][k], 0));
}
const tubeCtl = [
  [70, 186],
  [56, 96],
  [220, 150],
  [266, 30],
];
function tubeFrame(t: number) {
  const [x, y] = bez(t, tubeCtl);
  const [x2, y2] = bez(Math.min(1, t + 0.01), tubeCtl);
  const [x0, y0] = bez(Math.max(0, t - 0.01), tubeCtl);
  const dx = x2 - x0;
  const dy = y2 - y0;
  const l = Math.hypot(dx, dy) || 1;
  return { x, y, nx: -dy / l, ny: dx / l, w: 26 * (1 - t * 0.78), a: (Math.atan2(dy, dx) * 180) / Math.PI };
}
const tubeLine = (k: number, wobble = 0) =>
  Array.from({ length: 33 }, (_, i) => {
    const t = i / 32;
    const f = tubeFrame(t);
    const o = f.w * (k + wobble * Math.sin(t * 9 + k * 5) * 0.12);
    return `${i ? 'L' : 'M'}${r1(f.x + f.nx * o)} ${r1(f.y + f.ny * o)}`;
  }).join(' ');

export function TubeArt() {
  const c = category.Grow;
  return (
    <svg aria-hidden="true" viewBox={VB} className="art size-full" fill="none">
      <path d={tubeLine(1)} stroke={c.hue} strokeWidth="1.5" strokeDasharray="5 4" />
      <path d={tubeLine(-1)} stroke={c.hue} strokeWidth="1.5" strokeDasharray="5 4" />
      {[0.08, 0.36, 0.64, 0.9].map((t) => {
        const f = tubeFrame(t);
        return (
          <ellipse
            key={t}
            cx={r1(f.x)}
            cy={r1(f.y)}
            rx={r1(f.w * 0.36)}
            ry={r1(f.w)}
            transform={`rotate(${r1(f.a)} ${r1(f.x)} ${r1(f.y)})`}
            stroke={c.ink}
            strokeWidth="1.4"
            fill={c.tint}
          />
        );
      })}
      {[-0.75, -0.5, -0.25, 0, 0.25, 0.5, 0.75].map((k, i) => (
        <path
          key={k}
          d={tubeLine(k, 1)}
          pathLength={1}
          className="art-draw"
          style={style({ animationDelay: `${i * 0.12}s` })}
          stroke={i % 3 === 1 ? c.ink : '#9a9ea7'}
          strokeWidth="1.5"
          strokeLinecap="round"
        />
      ))}
      <Scalp />
    </svg>
  );
}

/* ------------------------------------------------------------------ cards */
const cardCtl = (dx: number, lean: number) => [
  [120 + dx, scalpY(120 + dx)],
  [118 + dx, 110],
  [150 + dx + lean, 70],
  [200 + dx + lean * 2, 40],
];
const cardStrand = (dx: number, lean: number, off: number) =>
  Array.from({ length: 21 }, (_, i) => {
    const t = i / 20;
    const [x, y] = bez(t, cardCtl(dx, lean));
    return `${i ? 'L' : 'M'}${r1(x + off * (1 - t * 0.6))} ${r1(y)}`;
  }).join(' ');
/** A card: a ribbon along the clump's middle strand, tapering, with its UV rows. */
function cardRibbon(dx: number, lean: number, w: number) {
  const pts = Array.from({ length: 11 }, (_, i) => {
    const t = i / 10;
    const [x, y] = bez(t, cardCtl(dx, lean));
    return { x, y, w: w * (1 - t * 0.55) };
  });
  const left = pts.map((p) => `${r1(p.x - p.w)} ${r1(p.y)}`);
  const right = pts.map((p) => `${r1(p.x + p.w)} ${r1(p.y)}`).reverse();
  return {
    outline: `M${left.join(' L')} L${right.join(' L')} Z`,
    rungs: pts.filter((_, i) => i % 2 === 0 && i > 0).map((p) => `M${r1(p.x - p.w)} ${r1(p.y)} L${r1(p.x + p.w)} ${r1(p.y)}`),
    spine: pts.map((p, i) => `${i ? 'L' : 'M'}${r1(p.x)} ${r1(p.y)}`).join(' '),
  };
}
const cardClumps = [
  { dx: -70, lean: -10 },
  { dx: 0, lean: 0 },
  { dx: 70, lean: 12 },
];

export function CardsArt() {
  const c = category.Games;
  return (
    <svg aria-hidden="true" viewBox={VB} className="art size-full" fill="none">
      <g className="art-fade-a">
        {cardClumps.map((k) =>
          [-10, -5, 0, 5, 10].map((off) => (
            <path key={`${k.dx}${off}`} d={cardStrand(k.dx, k.lean, off)} stroke="#9a9ea7" strokeWidth="1.3" strokeLinecap="round" />
          )),
        )}
      </g>
      <g className="art-fade-b">
        {cardClumps.map((k) => {
          const r = cardRibbon(k.dx, k.lean, 13);
          return (
            <g key={k.dx}>
              <path d={r.outline} fill={c.tint} stroke={c.ink} strokeWidth="1.4" strokeLinejoin="round" />
              {r.rungs.map((d) => (
                <path key={d} d={d} stroke={c.hue} strokeWidth="1" />
              ))}
              <path d={r.spine} stroke={c.hue} strokeWidth="1" strokeDasharray="3 3" />
            </g>
          );
        })}
      </g>
      <Scalp />
    </svg>
  );
}

/* ------------------------------------------------------------------ simulate */
const simStrands = Array.from({ length: 8 }, (_, i) => {
  const x = 58 + i * 29;
  return { x, y: scalpY(x), seg: 30 + ((i * 13) % 9), delay: -i * 0.22 };
});

export function SimulateArt() {
  const c = category.Motion;
  return (
    <svg aria-hidden="true" viewBox={VB} className="art size-full" fill="none" strokeLinecap="round">
      {[48, 78, 108].map((y, i) => (
        <path
          key={y}
          d={`M${20 + i * 30} ${y} h46`}
          className="art-wind"
          style={style({ animationDelay: `${-i * 0.7}s` })}
          stroke={c.hue}
          strokeWidth="1.5"
          strokeDasharray="10 8"
        />
      ))}
      {simStrands.map((s, i) => {
        const L = s.seg;
        const seg = (k: number) => `M0 0 C1 ${-L * 0.4} ${k * 2} ${-L * 0.8} ${k * 3} ${-L}`;
        const stroke = i % 3 === 1 ? c.ink : '#9a9ea7';
        return (
          <g key={i} transform={`translate(${s.x} ${r1(s.y)})`}>
            <g className="art-swing" style={style({ animationDelay: `${s.delay}s`, '--swing': '5deg' })}>
              <path d={seg(1)} stroke={stroke} strokeWidth="1.8" />
              <g transform={`translate(3 ${-L})`}>
                <g className="art-swing" style={style({ animationDelay: `${s.delay - 0.25}s`, '--swing': '9deg' })}>
                  <path d={seg(2)} stroke={stroke} strokeWidth="1.6" />
                  <g transform={`translate(6 ${-L})`}>
                    <g className="art-swing" style={style({ animationDelay: `${s.delay - 0.5}s`, '--swing': '14deg' })}>
                      <path d={seg(3)} stroke={stroke} strokeWidth="1.4" />
                      <circle cx="9" cy={-L} r="2" fill={stroke} />
                    </g>
                  </g>
                </g>
              </g>
            </g>
          </g>
        );
      })}
      <Scalp />
      {/* Root hold: the strands are pinned where they leave the scalp. */}
      {simStrands.map((s, i) => (
        <circle key={i} cx={s.x} cy={r1(s.y)} r="2.6" fill="#0b0c0f" stroke={c.ink} strokeWidth="1.4" />
      ))}
    </svg>
  );
}

/* ------------------------------------------------------------------ feathers */
const featherCtl = [
  [84, 190],
  [80, 120],
  [110, 70],
  [176, 22],
];
const barbs = Array.from({ length: 30 }, (_, i) => {
  const t = 0.12 + (i / 29) * 0.84;
  const [x, y] = bez(t, featherCtl);
  const [x2, y2] = bez(t + 0.01, featherCtl);
  const dx = x2 - x;
  const dy = y2 - y;
  const l = Math.hypot(dx, dy) || 1;
  const tx = dx / l;
  const ty = dy / l;
  const env = Math.sin(Math.PI * Math.min(1, (t - 0.1) / 0.9)) ** 0.7;
  const len = 46 * env;
  return [1, -1].map((side) => {
    // Barbs leave the rachis angled toward the tip.
    const nx = -ty * side;
    const ny = tx * side;
    const ex = x + (nx * 0.8 + tx * 0.6) * len;
    const ey = y + (ny * 0.8 + ty * 0.6) * len;
    return `M${r1(x)} ${r1(y)} Q${r1(x + nx * len * 0.5)} ${r1(y + ny * len * 0.5)} ${r1(ex)} ${r1(ey)}`;
  });
}).flat();
const rachis = Array.from({ length: 21 }, (_, i) => {
  const [x, y] = bez(i / 20, featherCtl);
  return `${i ? 'L' : 'M'}${r1(x)} ${r1(y)}`;
}).join(' ');
const scales = Array.from({ length: 4 }, (_, row) =>
  Array.from({ length: 4 - (row % 2) }, (_, col) => ({ x: 222 + col * 22 + (row % 2) * 11, y: 70 + row * 20, i: row * 4 + col })),
).flat();

export function FeathersArt() {
  const c = category.Feathers;
  return (
    <svg aria-hidden="true" viewBox={VB} className="art size-full" fill="none" strokeLinecap="round">
      <g className="art-sway" style={style({ transformOrigin: '84px 190px' })}>
        {barbs.map((d, i) => (
          <path key={i} d={d} stroke={i % 2 ? c.hue : '#6d717a'} strokeWidth="1.1" />
        ))}
        <path d={rachis} stroke={c.ink} strokeWidth="2.2" />
      </g>
      {scales
        .slice()
        .reverse()
        .map((s) => (
          <path
            key={s.i}
            className="art-pop"
            style={style({ transformOrigin: `${s.x}px ${s.y}px`, animationDelay: `${s.i * 0.12}s` })}
            d={`M${s.x - 12} ${s.y} Q${s.x - 12} ${s.y + 22} ${s.x} ${s.y + 24} Q${s.x + 12} ${s.y + 22} ${s.x + 12} ${s.y} Z`}
            fill={s.i % 3 === 0 ? c.tint : '#16181d'}
            stroke={c.ink}
            strokeWidth="1.3"
          />
        ))}
    </svg>
  );
}

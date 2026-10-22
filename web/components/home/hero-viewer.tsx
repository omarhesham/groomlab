// SPDX-License-Identifier: Apache-2.0
'use client';
import { Move } from 'lucide-react';
import { useCallback, useRef } from 'react';
import type { GroomStats } from './groom-3d';
import type { StrandParams } from './strand-engine';
import { StrandViewport } from './strand-viewport';

/*
 * The hero's head of curls: big, in 3D, standing on the page with no frame. A glass chip at the top
 * right reads two of its params as they breathe (Curl's radius and Interpolate's length) and the
 * strands it draws; under it on the right, a hint that a drag turns it.
 */

const params: StrandParams = {
  density: 1000,
  clump: 0.86,
  clumpCount: 110,
  curl: 0.42,
  curlFrequency: 2,
  noise: 0.1,
  color: { melanin: 0.66, redness: 0.5 },
  showGuides: false,
  seed: 7,
  length: 1.8,
  volume: 0.85,
};

/** The two params the chip reads, at time t (seconds): a slow breath, out of step with each other. */
function breathe(t: number) {
  return {
    curl: 0.42 + 0.1 * Math.sin(t * 0.32),
    length: 1.8 + 0.1 * Math.sin(t * 0.21 + 1.3),
  };
}

export function HeroViewer() {
  const curlRef = useRef<HTMLSpanElement>(null);
  const lengthRef = useRef<HTMLSpanElement>(null);
  const countRef = useRef<HTMLSpanElement>(null);
  const live = useCallback((t: number) => {
    const b = breathe(t);
    if (curlRef.current) curlRef.current.textContent = b.curl.toFixed(2);
    if (lengthRef.current) lengthRef.current.textContent = b.length.toFixed(1);
    return b;
  }, []);
  const onStats = useCallback((s: GroomStats) => {
    if (countRef.current) countRef.current.textContent = `${s.strands.toLocaleString('en-US')} strands`;
  }, []);

  return (
    <div className="relative h-[min(104vw,460px)] sm:h-[clamp(460px,52vw,560px)] lg:h-[clamp(520px,44vw,640px)]">
      <StrandViewport
        params={params}
        live={live}
        strandScale={12}
        spin={0.16}
        pitch={0.14}
        fill={1}
        yaw={2.45}
        skin={[0.17, 0.16, 0.16]}
        framing={[[0, 0.2, 0], 2.3]}
        onStats={onStats}
        label="A head of brown curls in 3D, turning slowly; drag to turn it"
      />
      {/* The params, live. */}
      <div
        aria-hidden="true"
        className="pointer-events-none absolute top-3 right-[var(--gutter)] rounded-xl border border-white/[0.12] bg-[#16181d]/60 px-4 py-3 text-[13px] leading-[1.7] text-gl-ink-2 shadow-[0_10px_30px_-12px_rgba(0,0,0,0.6)] backdrop-blur-md sm:top-6 sm:right-0"
      >
        <div className="flex items-center gap-3.5">
          <span className="size-2.5 rounded-full bg-gl-blue shadow-[0_0_10px_rgba(47,123,246,0.7)]" />
          <span className="grid grid-cols-[auto_auto] gap-x-2">
            <span>
              Curl <span className="text-gl-ink-3">·</span>
            </span>
            <span ref={curlRef} className="text-gl-ink tabular-nums">
              {params.curl.toFixed(2)}
            </span>
            <span>
              Length <span className="text-gl-ink-3">·</span>
            </span>
            <span ref={lengthRef} className="text-gl-ink tabular-nums">
              {(params.length ?? 1).toFixed(1)}
            </span>
          </span>
        </div>
        <span ref={countRef} className="mt-1.5 block border-t border-white/[0.08] pt-1.5 font-mono text-[11px] text-gl-ink-3 tabular-nums" />
      </div>
      {/* A drag turns it. */}
      <div
        aria-hidden="true"
        className="pointer-events-none absolute right-[var(--gutter)] bottom-2 flex flex-col items-center gap-3 sm:right-0 sm:bottom-6"
      >
        <span className="grid size-11 place-items-center rounded-full border border-white/25 text-gl-ink">
          <Move className="size-[18px]" strokeWidth={1.6} />
        </span>
        <span className="t-caps text-[10px] text-gl-ink-3">Drag to rotate</span>
      </div>
    </div>
  );
}

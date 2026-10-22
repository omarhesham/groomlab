// SPDX-License-Identifier: Apache-2.0
'use client';
import { Move } from 'lucide-react';
import type { GroomControl } from '@/lib/groomlab-wasm';
import { useCallback, useMemo, useRef, useState } from 'react';
import { HERO_CHIP, HERO_GROOM } from './engine-grooms';
import type { GroomStats } from './strand-3d';
import { StrandViewport } from './strand-viewport';

/*
 * The hero's head of curls: big, in 3D, standing on the page with no frame, grown once by the
 * GroomLab engine in the browser (manny_curls.groom; the bare head shows while it loads).
 * A glass chip at the top right reads two of the groom's params (Curl's radius and its length
 * scale, a Transform node's) and the strands it draws; under it on the right, a hint that a drag
 * turns it.
 */

export function HeroViewer() {
  const countRef = useRef<HTMLSpanElement>(null);
  const onStats = useCallback((s: GroomStats) => {
    if (countRef.current) countRef.current.textContent = `${s.strands.toLocaleString('en-US')} strands`;
  }, []);
  // The chip's values: the groom's own, as the engine reads them once it has loaded.
  const [chip, setChip] = useState({ curl: HERO_CHIP.curl.value, length: HERO_CHIP.length.value });
  const engine = useMemo(
    () => ({
      source: HERO_GROOM,
      onControls: (controls: GroomControl[]) => {
        const value = (id: string, v: number) => controls.find((c) => c.id === id)?.value ?? v;
        setChip({ curl: value(HERO_CHIP.curl.id, HERO_CHIP.curl.value), length: value(HERO_CHIP.length.id, HERO_CHIP.length.value) });
      },
    }),
    [],
  );

  return (
    <div className="relative h-[min(104vw,460px)] sm:h-[clamp(460px,52vw,560px)] lg:h-[clamp(520px,44vw,640px)]">
      <StrandViewport
        pitch={0.14}
        fill={1}
        yaw={2.45}
        skin={[0.17, 0.16, 0.16]}
        framing={[[0, 0.2, 0], 2.3]}
        onStats={onStats}
        engine={engine}
        label="A head of brown curls in 3D; drag to turn it"
      />
      {/* The params it was grown with. */}
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
            <span className="text-gl-ink tabular-nums">
              {chip.curl.toFixed(2)}
            </span>
            <span>
              Length <span className="text-gl-ink-3">·</span>
            </span>
            <span className="text-gl-ink tabular-nums">
              {chip.length.toFixed(1)}
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

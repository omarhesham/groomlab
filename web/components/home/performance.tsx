// SPDX-License-Identifier: Apache-2.0
import type { CSSProperties } from 'react';
import { Reveal } from '@/components/reveal';
import { TextLink } from '@/components/text-link';
import { cn } from '@/lib/cn';
import {
  benchmarkInfo,
  benchmarkRows,
  factor,
  formatFactor,
  formatValue,
  groomlabValue,
  type BenchmarkRow,
} from '@/lib/benchmark';
import { accentColor, accentText } from './feature-section';
import { Curl } from './strands';

const COUNT = '500k' as const;

/* The landing page's rows. */
const headline = benchmarkRows.filter((r) => r.home && factor(r, COUNT) !== undefined);

export function Performance() {
  return (
    <section aria-labelledby="perf-title" className="section-y">
      <div className="wrap">
        <Reveal className="mx-auto max-w-3xl text-center">
          <p className={cn('t-eyebrow flex items-center justify-center gap-2', accentText.emerald)}>
            <Curl color={accentColor.emerald} className="size-5" />
            Performance
          </p>
          <h2 id="perf-title" className="display t-section mt-4">
            Half a million strands. <span className="text-gl-ink-3">Measured against XGen.</span>
          </h2>
          <p className="t-lede mt-6 text-gl-ink-2">Same scalp, same machine, 500,000 strands.</p>
        </Reveal>

        <Reveal className="mt-14 sm:mt-16">
          <ul className="grid gap-x-12 gap-y-9 md:grid-cols-2">
            {headline.map((row, i) => (
              <li key={row.id}>
                <Comparison row={row} index={i} />
              </li>
            ))}
          </ul>
        </Reveal>

        <Reveal className="mt-14 max-w-3xl border-t border-gl-line pt-8 text-[13px] leading-relaxed text-gl-ink-3 sm:mt-16">
          <p>
            {benchmarkInfo.host}; {benchmarkInfo.machine}. {benchmarkInfo.setup} {benchmarkInfo.runs}
          </p>
          <TextLink href="/docs/performance" className="mt-4 text-[1rem]">
            Every number, and how to run it yourself
          </TextLink>
        </Reveal>
      </div>
    </section>
  );
}

/** A measure as two bars, XGen's and GroomLab's, with how many times better GroomLab is. */
function Comparison({ row, index }: { row: BenchmarkRow; index: number }) {
  const xgen = row.values[COUNT].xgen!;
  const ours = groomlabValue(row, COUNT)!;
  const most = Math.max(xgen, ours);
  const f = factor(row, COUNT)!;
  const variant = row.compare === 'follow' ? 'GroomLab, Follow Scalp' : row.compare === 'nocurl' ? 'GroomLab, no Curl' : row.compare === 'gpu' ? 'GroomLab, GPU preview' : 'GroomLab';
  return (
    <figure>
      <figcaption className="flex items-baseline justify-between gap-4">
        <span className="text-[17px] font-semibold tracking-tight">{row.label}</span>
        <span className={cn('shrink-0 text-[15px] font-semibold tabular-nums', accentText.emerald)}>
          {formatFactor(f)} {row.better}
        </span>
      </figcaption>
      <dl className="mt-3 space-y-2">
        <Bar label="XGen" value={formatValue(xgen, row.unit)} share={xgen / most} color="var(--color-gl-line)" delay={index * 0.08} />
        <Bar label={variant} value={formatValue(ours, row.unit)} share={ours / most} color={accentColor.emerald} delay={index * 0.08 + 0.15} />
      </dl>
    </figure>
  );
}

function Bar({ label, value, share, color, delay }: { label: string; value: string; share: number; color: string; delay: number }) {
  return (
    <div className="grid grid-cols-[minmax(0,1fr)_auto] items-center gap-x-3 gap-y-1">
      <dt className="text-[13px] text-gl-ink-2">{label}</dt>
      <dd className="text-[13px] font-medium tabular-nums text-gl-ink">{value}</dd>
      <dd className="col-span-2 h-2.5 overflow-hidden rounded-full bg-gl-mist">
        <span
          className="bar-grow block h-full rounded-full"
          style={{ width: `${Math.max(share * 100, 1.5)}%`, background: color, '--d': `${delay}s` } as CSSProperties}
        />
      </dd>
    </div>
  );
}

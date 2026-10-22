// SPDX-License-Identifier: Apache-2.0
import type { CSSProperties } from 'react';
import { Reveal } from '@/components/reveal';
import { TextLink } from '@/components/text-link';
import {
  benchmarkInfo,
  benchmarkRows,
  factor,
  formatValue,
  groomlabValue,
  type BenchmarkRow,
} from '@/lib/benchmark';
import { CountUp } from './count-up';
import { category } from './graph-theme';
import { SectionHead } from './section-node';

const COUNT = '500k' as const;
const headline = benchmarkRows.filter((r) => r.home && factor(r, COUNT) !== undefined);
const green = category.Sources;

/** Performance, shown as the Attribute Editor would: a panel of measured values, bars filling in. */
export function Inspector() {
  return (
    <section aria-labelledby="perf-title" className="section-y">
      <div className="wrap">
        <SectionHead
          id="perf-title"
          cat="Fields"
          index="04"
          tag="Performance"
          title="Half a million strands."
          muted="Measured against XGen."
          lede={<p>Same scalp, same machine, 500,000 strands.</p>}
        />

        <Reveal className="mt-14 sm:mt-16">
          <div className="overflow-hidden rounded-[14px] border border-white/[0.08]">
            <div className="flex flex-wrap items-center gap-x-4 gap-y-1 border-b border-white/[0.08] bg-gl-panel px-4 py-2.5 font-mono text-[11.5px] text-gl-ink-2">
              <span className="flex items-center gap-2 font-sans text-[13px] font-semibold text-gl-ink">
                Inspector
              </span>
              <span className="rounded-md bg-gl-paper px-2 py-0.5 ring-1 ring-white/10">benchmark</span>
              <span>500k strands</span>
              <span className="ml-auto hidden sm:inline">XGen vs GroomLab · {benchmarkInfo.host}</span>
            </div>
            <ul className="grid md:grid-cols-2">
              {headline.map((row, i) => (
                <li
                  key={row.id}
                  className="border-white/[0.07] p-5 not-last:border-b sm:p-6 md:[&:nth-child(odd)]:border-r md:[&:nth-last-child(-n+2)]:border-b-0"
                >
                  <Measure row={row} index={i} />
                </li>
              ))}
            </ul>
            <div className="border-t border-white/[0.08] bg-gl-panel px-5 py-4 text-[13px] leading-relaxed text-gl-ink-3 sm:px-6">
              <p>
                {benchmarkInfo.host}; {benchmarkInfo.machine}. {benchmarkInfo.setup} {benchmarkInfo.runs}
              </p>
            </div>
          </div>
          <TextLink href="/docs/performance" className="mt-6">
            Every number, and how to run it yourself
          </TextLink>
        </Reveal>
      </div>
    </section>
  );
}

function Measure({ row, index }: { row: BenchmarkRow; index: number }) {
  const xgen = row.values[COUNT].xgen!;
  const ours = groomlabValue(row, COUNT)!;
  const most = Math.max(xgen, ours);
  const f = factor(row, COUNT)!;
  const variant =
    row.compare === 'follow'
      ? 'GroomLab, Follow Scalp'
      : row.compare === 'nocurl'
        ? 'GroomLab, no Curl'
        : row.compare === 'gpu'
          ? 'GroomLab, GPU preview'
          : 'GroomLab';
  return (
    <figure>
      <figcaption className="flex items-start justify-between gap-4">
        <span className="text-[15px] font-semibold tracking-tight sm:text-[16px]">{row.label}</span>
        <span className="shrink-0 text-right leading-none" style={{ color: green.ink }}>
          <span className="block text-[30px] font-semibold tracking-tight sm:text-[34px]">
            <CountUp value={f >= 10 ? Math.round(f) : Number(f.toFixed(1))} decimals={f >= 10 ? 0 : 1} suffix="×" />
          </span>
          <span className="mt-1 block font-mono text-[11px] text-gl-ink-2">{row.better}</span>
        </span>
      </figcaption>
      <dl className="mt-4 space-y-1.5">
        <Bar label="XGen" value={formatValue(xgen, row.unit)} share={xgen / most} color="#8a8f99" delay={index * 0.08} />
        <Bar label={variant} value={formatValue(ours, row.unit)} share={ours / most} color={green.hue} delay={index * 0.08 + 0.15} />
      </dl>
    </figure>
  );
}

/** A ValueSlider-style readout: the field fills to the share, the label and value inside it. */
function Bar({ label, value, share, color, delay }: { label: string; value: string; share: number; color: string; delay: number }) {
  return (
    <div className="relative flex h-7 items-center justify-between overflow-hidden rounded-[7px] bg-[#1a1c21] px-2.5 font-mono text-[12px]">
      <span
        aria-hidden="true"
        className="bar-grow absolute inset-y-0 left-0 opacity-45"
        style={{ width: `${Math.max(share * 100, 1.2)}%`, background: color, '--d': `${delay}s` } as CSSProperties}
      />
      <dt className="relative text-gl-ink-2">{label}</dt>
      <dd className="relative tabular-nums text-gl-ink">{value}</dd>
    </div>
  );
}

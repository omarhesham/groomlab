// SPDX-License-Identifier: Apache-2.0
'use client';
import { Eye, EyeOff, RotateCcw, Shuffle } from 'lucide-react';
import { useId, useMemo, useState, type CSSProperties, type ReactNode } from 'react';
import { cn } from '@/lib/cn';
import { category, port, type Category, type PortType } from './graph-theme';
import { defaultParams, type StrandParams } from './strand-engine';
import { StrandViewport } from './strand-viewport';

/*
 * "Try the node graph": a short chain of real GroomLab nodes with their real param names. Bypass a
 * node or drag a slider (the app's ValueSlider: the field fills as it is dragged) and the strands
 * in the viewport are grown again on the spot.
 */

interface State {
  density: number;
  showGuides: boolean;
  clumpOn: boolean;
  strength: number;
  count: number;
  curlOn: boolean;
  radius: number;
  frequency: number;
  noiseOn: boolean;
  amplitude: number;
  colorOn: boolean;
  melanin: number;
  redness: number;
  seed: number;
}

const initial: State = {
  density: defaultParams.density,
  showGuides: false,
  clumpOn: true,
  strength: defaultParams.clump,
  count: defaultParams.clumpCount,
  curlOn: true,
  radius: defaultParams.curl,
  frequency: defaultParams.curlFrequency,
  noiseOn: true,
  amplitude: defaultParams.noise,
  colorOn: true,
  melanin: 0.55,
  redness: 0.45,
  seed: defaultParams.seed,
};

export function TryGraph() {
  const [s, setS] = useState<State>(initial);
  const set = <K extends keyof State>(k: K) => (v: State[K]) => setS((prev) => ({ ...prev, [k]: v }));

  const params = useMemo<StrandParams>(
    () => ({
      density: s.density,
      clump: s.clumpOn ? s.strength : 0,
      clumpCount: s.count,
      curl: s.curlOn ? s.radius : 0,
      curlFrequency: s.frequency,
      noise: s.noiseOn ? s.amplitude : 0,
      color: s.colorOn ? { melanin: s.melanin, redness: s.redness } : null,
      showGuides: s.showGuides,
      seed: s.seed,
    }),
    [s],
  );

  return (
    <div className="mt-14 grid gap-6 sm:mt-16 lg:grid-cols-12 lg:gap-10">
      {/* The viewport: first (and pinned under the header) on phones, beside the nodes on wide screens. */}
      <div className="sticky top-14 z-10 lg:order-2 lg:col-span-7 lg:self-start lg:top-20">
        <div className="overflow-hidden rounded-[18px] bg-[#f6f6f8]">
          <div className="flex h-10 items-center justify-between gap-3 border-b border-[#ebebef] px-3 font-mono text-[11.5px] text-gl-ink-2">
            <span className="flex items-center gap-2">
              <span aria-hidden="true" className="size-2 rounded-full bg-gl-ink" />
              Output · persp
            </span>
            <span className="flex items-center gap-1">
              <span className="mr-2 tabular-nums" aria-live="polite">
                {s.density} strands
              </span>
              <IconButton label="New random roots" onClick={() => set('seed')(s.seed + 1)}>
                <Shuffle className="size-3.5" strokeWidth={2.2} />
              </IconButton>
              <IconButton label="Reset the graph" onClick={() => setS(initial)}>
                <RotateCcw className="size-3.5" strokeWidth={2.2} />
              </IconButton>
            </span>
          </div>
          <div className="h-[min(42svh,320px)] bg-[#fafafb] lg:h-auto lg:aspect-[7/6]">
            <StrandViewport
              params={params}
              label="The groom the graph makes, redrawn as the nodes change"
            />
          </div>
        </div>
      </div>

      <ol className="lg:order-1 lg:col-span-5" aria-label="Nodes">
        <Node cat="Sources" name="Scalp" out="mesh">
          <Static label="mesh" value="head" />
        </Node>
        <Node cat="Grow" name="Scatter" out="strands">
          <ValueSlider cat="Grow" label="density" value={s.density} min={40} max={800} step={10} onChange={set('density')} format={(v) => String(v)} />
        </Node>
        <Node
          cat="Grow"
          name="Interpolate"
          out="strands"
          action={
            <IconButton label="Show the guides" pressed={s.showGuides} onClick={() => set('showGuides')(!s.showGuides)}>
              {s.showGuides ? <Eye className="size-4" strokeWidth={2.2} /> : <EyeOff className="size-4" strokeWidth={2.2} />}
            </IconButton>
          }
        >
          <Static label="guides" value="9" />
        </Node>
        <Node cat="Look" name="Clump" out="strands" on={s.clumpOn} onToggle={set('clumpOn')}>
          <ValueSlider cat="Look" label="strength" value={s.strength} min={0} max={1} step={0.01} onChange={set('strength')} />
          <ValueSlider cat="Look" label="count" value={s.count} min={4} max={40} step={1} onChange={set('count')} format={(v) => String(v)} />
        </Node>
        <Node cat="Look" name="Curl" out="strands" on={s.curlOn} onToggle={set('curlOn')}>
          <ValueSlider cat="Look" label="radius" value={s.radius} min={0} max={1} step={0.01} onChange={set('radius')} />
          <ValueSlider cat="Look" label="frequency" value={s.frequency} min={1} max={8} step={0.5} onChange={set('frequency')} format={(v) => v.toFixed(1)} />
        </Node>
        <Node cat="Look" name="Noise" out="strands" on={s.noiseOn} onToggle={set('noiseOn')}>
          <ValueSlider cat="Look" label="amplitude" value={s.amplitude} min={0} max={1} step={0.01} onChange={set('amplitude')} />
        </Node>
        <Node cat="Color" name="HairColor" out="strands" on={s.colorOn} onToggle={set('colorOn')}>
          <ValueSlider cat="Color" label="melanin" value={s.melanin} min={0} max={1} step={0.01} onChange={set('melanin')} />
          <ValueSlider cat="Color" label="redness" value={s.redness} min={0} max={1} step={0.01} onChange={set('redness')} />
        </Node>
        <Node cat="Output" name="Output" last>
          <Static label="to the viewport" value={`${s.density} strands`} />
        </Node>
      </ol>
    </div>
  );
}

/** A node card, its input port on top and the wire to the next node below it. */
function Node({
  cat,
  name,
  out,
  on = true,
  onToggle,
  action,
  last,
  children,
}: {
  cat: Category;
  name: string;
  out?: PortType;
  on?: boolean;
  onToggle?: (v: boolean) => void;
  action?: ReactNode;
  last?: boolean;
  children: ReactNode;
}) {
  const c = category[cat];
  const dark = cat === 'Output';
  return (
    <li className="relative">
      <div
        className={cn(
          'try-node relative rounded-[12px] border bg-white transition-[opacity,border-color,box-shadow] duration-300',
          on ? 'border-[#d9d9df] hover:border-[#c4c4cc] hover:shadow-[0_10px_24px_-18px_rgba(0,0,0,0.35)]' : 'border-dashed border-[#d0d0d6] opacity-60',
        )}
      >
        <div
          className="flex h-10 items-center gap-2.5 rounded-t-[11px] pr-1.5 pl-3 transition-colors duration-300"
          style={{ background: on ? c.hue : '#f2f2f4', color: on ? c.on : undefined }}
        >
          <span aria-hidden="true" className="size-2 rounded-full" style={{ background: on ? 'currentColor' : '#b8b8bf' }} />
          <span className="text-[14px] font-semibold tracking-tight">{name}</span>
          {!on && <span className="font-mono text-[11px] text-gl-ink-3">bypassed</span>}
          <span className="ml-auto flex items-center">
            {action}
            {onToggle && <Switch label={`${name} node on`} checked={on} hue={dark ? c.hue : c.on} onChange={onToggle} />}
          </span>
        </div>
        <div className="space-y-2 px-3 py-3">{children}</div>
      </div>
      {!last && out && <VWire color={on ? port[out] : '#c9c9d0'} />}
    </li>
  );
}

/** The wire between two stacked nodes, a pulse running down it. */
function VWire({ color }: { color: string }) {
  return (
    <div aria-hidden="true" className="relative ml-[17px] h-7 w-[10px]">
      <span className="absolute -top-[5px] left-0 size-[10px] rounded-full border-2 border-white" style={{ background: color }} />
      <span className="absolute inset-y-0 left-[4px] w-[2px] rounded-full opacity-60" style={{ background: color }} />
      <span className="vwire-pulse absolute top-0 left-[3px] h-2 w-[4px] rounded-full" style={{ background: color }} />
      <span className="absolute -bottom-[5px] left-0 size-[10px] rounded-full border-2 border-white" style={{ background: color }} />
    </div>
  );
}

function Static({ label, value }: { label: string; value: string }) {
  return (
    <p className="flex h-7 items-center justify-between rounded-[7px] bg-[#f4f4f6] px-2.5 font-mono text-[12px]">
      <span className="text-gl-ink-2">{label}</span>
      <span className="text-gl-ink">{value}</span>
    </p>
  );
}

/** The app's ValueSlider: a field that fills as it is dragged, its label and value inside. */
function ValueSlider({
  cat,
  label,
  value,
  min,
  max,
  step,
  onChange,
  format = (v) => v.toFixed(2),
}: {
  cat: Category;
  label: string;
  value: number;
  min: number;
  max: number;
  step: number;
  onChange: (v: number) => void;
  format?: (v: number) => string;
}) {
  const id = useId();
  const c = category[cat];
  const share = ((value - min) / (max - min)) * 100;
  return (
    <div
      className="value-slider relative h-7 overflow-hidden rounded-[7px] bg-[#f1f1f4] font-mono text-[12px] has-[:focus-visible]:outline-2 has-[:focus-visible]:outline-offset-2 has-[:focus-visible]:outline-gl-blue"
      style={{ '--fill': c.hue } as CSSProperties}
    >
      <span aria-hidden="true" className="absolute inset-y-0 left-0 opacity-60" style={{ width: `${share}%`, background: c.hue }} />
      <span aria-hidden="true" className="absolute inset-y-0 w-[2px]" style={{ left: `calc(${share}% - 1px)`, background: c.ink }} />
      <label htmlFor={id} className="pointer-events-none absolute inset-y-0 left-2.5 flex items-center text-gl-ink-2">
        {label}
      </label>
      <span aria-hidden="true" className="pointer-events-none absolute inset-y-0 right-2.5 flex items-center tabular-nums text-gl-ink">
        {format(value)}
      </span>
      <input
        id={id}
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        aria-valuetext={format(value)}
        onChange={(e) => onChange(Number(e.target.value))}
        className="absolute inset-0 size-full cursor-ew-resize appearance-none opacity-0"
      />
    </div>
  );
}

function Switch({ label, checked, hue, onChange }: { label: string; checked: boolean; hue: string; onChange: (v: boolean) => void }) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={checked}
      aria-label={label}
      onClick={() => onChange(!checked)}
      className="grid h-8 w-11 place-items-center rounded-full focus-visible:outline-2 focus-visible:outline-gl-blue"
    >
      <span
        className="relative h-[18px] w-[30px] rounded-full transition-colors duration-200"
        style={{ background: checked ? hue : '#d1d1d6' }}
      >
        <span
          className={cn(
            'absolute top-[2px] left-[2px] size-[14px] rounded-full bg-white shadow-sm transition-transform duration-300 ease-spring',
            checked && 'translate-x-[12px]',
          )}
        />
      </span>
    </button>
  );
}

function IconButton({
  label,
  pressed,
  onClick,
  children,
}: {
  label: string;
  pressed?: boolean;
  onClick: () => void;
  children: ReactNode;
}) {
  return (
    <button
      type="button"
      aria-label={label}
      aria-pressed={pressed}
      title={label}
      onClick={onClick}
      className={cn(
        'grid size-8 place-items-center rounded-full text-gl-ink-2 transition-colors hover:bg-black/[0.05] hover:text-gl-ink focus-visible:outline-2 focus-visible:outline-gl-blue',
        pressed && 'text-gl-blue-strong',
      )}
    >
      {children}
    </button>
  );
}

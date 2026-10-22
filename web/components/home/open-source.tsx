// SPDX-License-Identifier: Apache-2.0
import type { CSSProperties } from 'react';
import { Reveal } from '@/components/reveal';
import { pill } from '@/components/store-buttons';
import { TextLink } from '@/components/text-link';
import { cn } from '@/lib/cn';
import { GitHubMark } from '@/components/github-mark';
import { siteConfig } from '@/site.config';
import { SectionHead } from './section-node';
import { connect, SvgNode, Wire, type NodeSpec } from './svg-graph';

/*
 * Open source, as a Merge node: your branch and dev go in, dev comes out, and a release carries
 * dev into main (CONTRIBUTING.md: pull requests go to dev; main holds released versions).
 */

function wide(): NodeSpec[] {
  return [
    { id: 'pr', name: 'your-branch', mono: true, cat: 'Color', x: 0, y: 24, w: 160, rows: [{ label: 'pull request' }], outs: [{ type: 'field' }] },
    { id: 'dev', name: 'dev', mono: true, cat: 'Shape', x: 0, y: 160, w: 160, rows: [{ label: 'where work happens' }], outs: [{ type: 'guides' }] },
    {
      id: 'merge',
      name: 'Merge',
      cat: 'Utility',
      x: 220,
      y: 88,
      w: 140,
      rows: [{ label: 'into', value: 'dev' }],
      ins: [{ type: 'field' }, { type: 'guides', at: 49 }],
      outs: [{ type: 'guides' }],
    },
    {
      id: 'main',
      name: 'main',
      mono: true,
      cat: 'Output',
      x: 420,
      y: 88,
      w: 140,
      rows: [{ label: 'release', value: `v${siteConfig.version}` }],
      ins: [{ type: 'guides' }],
    },
  ];
}

function compact(): NodeSpec[] {
  return [
    { id: 'pr', name: 'your-branch', mono: true, cat: 'Color', x: 0, y: 0, w: 150, rows: [{ label: 'pull request' }], outs: [{ type: 'field', side: 'b' }] },
    { id: 'dev', name: 'dev', mono: true, cat: 'Shape', x: 170, y: 0, w: 150, rows: [{ label: 'work happens' }], outs: [{ type: 'guides', side: 'b' }] },
    {
      id: 'merge',
      name: 'Merge',
      cat: 'Utility',
      x: 90,
      y: 130,
      w: 140,
      rows: [{ label: 'into', value: 'dev' }],
      ins: [
        { type: 'field', side: 't', at: 40 },
        { type: 'guides', side: 't', at: 100 },
      ],
      outs: [{ type: 'guides', side: 'b' }],
    },
    {
      id: 'main',
      name: 'main',
      mono: true,
      cat: 'Output',
      x: 90,
      y: 250,
      w: 140,
      rows: [{ label: 'release', value: `v${siteConfig.version}` }],
      ins: [{ type: 'guides', side: 't' }],
    },
  ];
}

function MergeGraph({ nodes, vbw, vbh, className }: { nodes: NodeSpec[]; vbw: number; vbh: number; className?: string }) {
  const [pr, dev, merge, main] = nodes;
  const wires = [connect(pr, 0, merge, 0), connect(dev, 0, merge, 1), connect(merge, 0, main, 0)];
  return (
    <svg aria-hidden="true" viewBox={`0 0 ${vbw} ${vbh}`} className={className} overflow="visible">
      {wires.map((w, i) => (
        <Wire key={i} d={w.d} color={w.color} delay={i * 0.6} draw={0.2 + i * 0.25} />
      ))}
      {nodes.map((n, i) => (
        <g key={n.id} className="node-in" style={{ '--d': `${i * 0.08}s` } as CSSProperties}>
          <SvgNode n={n} />
        </g>
      ))}
    </svg>
  );
}

export function OpenSource() {
  return (
    <section aria-labelledby="oss-title" className="section-y">
      <div className="wrap">
        <SectionHead id="oss-title" cat="Utility" index="06" tag="Open source" title="Free and open source." muted="Built in the open." />
        <div className="mt-12 grid items-center gap-12 sm:mt-14 lg:grid-cols-12 lg:gap-16">
          <Reveal className="lg:col-span-5">
            <p className="t-lede text-gl-ink-2">
              Apache-2.0: every feature and every export, for any use. Pull requests go to{' '}
              <code className="font-mono text-[0.9em] text-gl-ink">dev</code>, and each release merges dev into{' '}
              <code className="font-mono text-[0.9em] text-gl-ink">main</code>.
            </p>
            <div className="mt-8 flex flex-col items-start gap-4 sm:flex-row sm:items-center sm:gap-7">
              <a href={siteConfig.repoUrl} className={cn(pill.base, pill.md, 'bg-gl-ink text-white hover:bg-black')}>
                <GitHubMark className="size-4" />
                Source on GitHub
              </a>
              <TextLink href={`${siteConfig.repoUrl}/blob/main/CONTRIBUTING.md`}>How to contribute</TextLink>
            </div>
          </Reveal>
          <Reveal delay={120} className="lg:col-span-7">
            <MergeGraph nodes={wide()} vbw={560} vbh={240} className="hidden h-auto w-full sm:block" />
            <MergeGraph nodes={compact()} vbw={320} vbh={320} className="mx-auto block h-auto w-full max-w-[340px] sm:hidden" />
          </Reveal>
        </div>
      </div>
    </section>
  );
}

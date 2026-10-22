// SPDX-License-Identifier: Apache-2.0
import { ArrowDownToLine, Heart, MessageCircle } from 'lucide-react';
import Link from 'next/link';
import { Reveal } from '@/components/reveal';
import { button } from '@/components/store-buttons';
import { GitHubMark } from '@/components/github-mark';
import { cn } from '@/lib/cn';
import { siteConfig } from '@/site.config';

/** The close: a full-width blue band, one download, and the two ways to stay close (Ko-fi, Discord). */
export function OutputCta() {
  const req = siteConfig.requirements;
  const ghost = 'text-white ring-1 ring-white/45 ring-inset hover:bg-white/12 hover:ring-white/70';
  return (
    <section id="get" aria-labelledby="get-title" className="bg-gl-blue-strong text-white">
      <Reveal className="wrap py-24 text-center sm:py-32">
        <h2 id="get-title" className="display t-section text-white">
          Start grooming.
        </h2>
        <p className="mx-auto mt-5 max-w-xl text-lg text-white/85">
          Free and open source, every feature, for any use.
        </p>
        <div className="mt-10 flex justify-center">
          <a
            href={siteConfig.downloadUrl}
            className={cn(button.base, button.lg, 'bg-white text-gl-blue-strong shadow-[0_14px_30px_-12px_rgba(0,0,0,0.35)] hover:bg-white/92')}
          >
            Download free
            <ArrowDownToLine aria-hidden="true" className="size-4 opacity-80" strokeWidth={2.4} />
          </a>
        </div>
        <div className="mt-4 flex flex-col items-stretch justify-center gap-3 sm:flex-row sm:items-center">
          <a href={siteConfig.donateUrl} target="_blank" rel="noopener noreferrer" className={cn(button.base, button.md, ghost)}>
            Support on Ko-fi
            <Heart aria-hidden="true" className="size-4 opacity-75" strokeWidth={2.4} />
          </a>
          <a href={siteConfig.discordUrl} target="_blank" rel="noopener noreferrer" className={cn(button.base, button.md, ghost)}>
            Join the Discord
            <MessageCircle aria-hidden="true" className="size-4 opacity-75" strokeWidth={2.4} />
          </a>
        </div>
        <p className="mt-10 text-sm text-white/70">
          {req.maya} on {req.os} <span aria-hidden="true">·</span>{' '}
          <Link className="underline-offset-2 hover:underline" href="/docs/installation">
            Install guide
          </Link>{' '}
          <span aria-hidden="true">·</span>{' '}
          <a className="inline-flex items-center gap-1.5 underline-offset-2 hover:underline" href={siteConfig.repoUrl}>
            <GitHubMark className="size-3.5" />
            Source on GitHub
          </a>
        </p>
      </Reveal>
    </section>
  );
}

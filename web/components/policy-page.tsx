// SPDX-License-Identifier: Apache-2.0
import type { ReactNode } from 'react';

/** The policy pages' layout (terms, privacy, refunds): a title, the date, plain sections. */
export function PolicyPage({ title, updated, intro, children }: { title: string; updated: string; intro: ReactNode; children: ReactNode }) {
  return (
    <article aria-labelledby="policy-title" className="wrap py-20 sm:py-24">
      <div className="mx-auto max-w-2xl">
        <h1 id="policy-title" className="display t-section">
          {title}
        </h1>
        <p className="mt-3 text-sm text-gl-ink-3">Last updated {updated}</p>
        <div className="mt-6 text-[17px] leading-relaxed text-gl-ink-2">{intro}</div>
        <div className="mt-10 space-y-9">{children}</div>
      </div>
    </article>
  );
}

/** One section of a policy page. */
export function PolicySection({ title, children }: { title: string; children: ReactNode }) {
  return (
    <section>
      <h2 className="text-xl font-semibold tracking-tight text-gl-ink">{title}</h2>
      <div className="mt-3 space-y-3 text-[15px] leading-relaxed text-gl-ink-2 [&_a]:text-gl-blue-strong [&_a:hover]:underline [&_li]:ml-5 [&_li]:list-disc [&_ul]:space-y-1.5">
        {children}
      </div>
    </section>
  );
}

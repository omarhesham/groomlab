// SPDX-License-Identifier: Apache-2.0
import { SiteFooter } from '@/components/home/site-footer';
import { SiteHeader } from '@/components/home/site-header';

/** The landing pages are always the dark studio (near-black, cream titles), whatever theme the docs use. */
export default function Layout({ children }: LayoutProps<'/'>) {
  return (
    <div data-landing="" className="flex flex-1 flex-col bg-gl-paper text-gl-ink [color-scheme:dark]">
      <a
        href="#main"
        className="sr-only z-50 rounded-[10px] bg-gl-blue-strong px-4 py-2 text-sm font-semibold text-white focus:not-sr-only focus:fixed focus:top-3 focus:left-3"
      >
        Skip to content
      </a>
      <SiteHeader />
      <main id="main" className="relative flex-1">
        {children}
      </main>
      <SiteFooter />
    </div>
  );
}

// SPDX-License-Identifier: Apache-2.0
import { SiteFooter } from '@/components/home/site-footer';
import { SiteHeader } from '@/components/home/site-header';

/** The landing pages are always paper-white (a light graph canvas), whatever theme the docs use. */
export default function Layout({ children }: LayoutProps<'/'>) {
  return (
    <div data-landing="" className="flex flex-1 flex-col bg-gl-paper text-gl-ink [color-scheme:light]">
      <a
        href="#main"
        className="sr-only z-50 rounded-full bg-gl-ink px-4 py-2 text-sm font-semibold text-gl-paper focus:not-sr-only focus:fixed focus:top-3 focus:left-3"
      >
        Skip to content
      </a>
      {/* The graph canvas's dot grid, drifting a little slower than the page (.canvas-grid). */}
      <div aria-hidden="true" className="canvas-grid" />
      <SiteHeader />
      <main id="main" className="relative flex-1">
        {children}
      </main>
      <SiteFooter />
    </div>
  );
}

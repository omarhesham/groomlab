# GroomLab website

The landing page and docs for GroomLab, built with [Next.js](https://nextjs.org) (App Router),
[Fumadocs](https://fumadocs.dev) (docs, sidebar and built-in Orama search) and Tailwind CSS 4.
It builds to a static site in `out/` that any host can serve.

## Commands

```bash
npm install        # once
npm run dev        # http://localhost:3000, live reload
npm run build      # static site in out/ (also type-checks)
npm run start      # serve out/ (npx serve out -l 4000 for another port)
npm run lint
npm run types:check
```

## Where things live

| Path | What |
| --- | --- |
| `site.config.ts` | Name, tagline, version, requirements, **store links and support email**. Change them here only. |
| `app/(home)/` | The landing page (always light: paper-white, ink type, one dark beat for game cards). |
| `components/home/` | Landing sections: `hero` (+ `hero-strands`, the combable tufts, and `works-with`), `features` (+ `feature-section`; the pinned cards tour and the motion flipbook), `examples` (+ `gallery`, pinned sideways scroll), `under-the-hood` (node rows, specs, Python card), `performance` (bars against XGen), `hosts` (where the engine runs), `cta-band`, header, footer. `strands` holds the SVG hair motif (`Curl`, `StrandFlow`). |
| `components/` | Shared pieces: `logo`, `store-buttons` (+ the squishy `pill` button styles), `text-link`, `example-image`, `reveal` (scroll fade-in), `mdx` (docs components), `app-shortcuts` (the app page's shortcut table, read at build time from `hosts/app/src/main_window.cpp`, the app's own Help > Keyboard Shortcuts page). |
| `lib/benchmark.ts` | The XGen benchmark's numbers (`tools/bench/xgen_compare/results/`), read by the landing page's performance section and the Performance docs page (`<BenchmarkTable count="500k" />`). Update numbers there only. |
| `content/docs/` | Docs pages (MDX). Sidebar order: `content/docs/meta.json`. |
| `app/docs/` | Docs layout and page (Fumadocs). |
| `app/api/search/` | The search index, generated at build time (static). |
| `app/global.css` | Brand palette (`gl-*` colors) and the docs theme in light and dark. |
| `public/examples/` | Example renders and screenshots (produced from Maya). |

## Motion

Motion lives in `app/global.css` and is CSS first: strands are SVG paths with `pathLength="1"` that
draw by dash offset; scroll-linked effects (hero zoom, parallax, strands that grow as you scroll,
the header's progress strand, the pinned cards tour) use CSS scroll timelines where the browser has
them, and fall back to a one-shot reveal (`Reveal`, an IntersectionObserver) or a static layout
where it does not. The only per-frame scripts are the hero tufts' springs (they stop when off
screen) and the examples gallery, which pins and moves sideways as the page scrolls. With
`prefers-reduced-motion: reduce` everything is static and fully drawn, the gallery is a swipeable
row and the node rows wrap instead of drifting.

## Example images

The site reads `public/examples/*.png` **at build time**. A missing file is not an error: the
frame shows a soft gradient with the image's caption instead, and optional images
(`motion-strip.png`, `reference.png`) are left out. Rebuild after adding images.

Expected files: `hero.png`, `viewport-guides.png`, `groom-tools.png`, `graph-editor.png`,
`paint-maps.png`, `game-cards.png`, `hairstyle.png`, `fur-creature.png`, `feathers.png`, and
optionally `motion-strip.png` and `reference.png`.

In docs pages, use `<Example name="file.png" alt="..." caption="..." />` (add `optional` to hide it
when the file is missing).

## Deploy

- **Vercel:** import the repository, set the project's root directory to `web`. The defaults
  (`npm run build`, output from `next.config.mjs`) work as they are.
- **Any static host** (Netlify, GitHub Pages, S3, nginx): run `npm run build` and upload `out/`.

Set `NEXT_PUBLIC_SITE_URL` to the site's address (e.g. `https://example.com`) when building for
production, so social preview images get absolute URLs.

## What's new

`content/docs/whats-new.mdx` lists the notable improvements, newest first; the hero's early-access
badge links to it. Add to it when a change alters how artists work.

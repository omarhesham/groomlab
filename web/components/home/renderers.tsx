// SPDX-License-Identifier: Apache-2.0
import Link from 'next/link';
import { Reveal } from '@/components/reveal';

// A renderer without a logo here shows its name. The Chaos V-Ray logo is used with Chaos's
// permission (granted for this site, 2026-10).
const renderers: { name: string; logo?: string }[] = [
  { name: 'Arnold', logo: '/renderers/Autodesk_Arnold_Logo.svg' },
  { name: 'RenderMan', logo: '/renderers/RenderMan_Logo.png' },
  { name: 'V-Ray', logo: '/renderers/Chaos_VRay_Logo.png' },
];

/** The renderers GroomLab renders in, and the way to the rest. */
export function Renderers() {
  return (
    <section aria-labelledby="renderers-title" className="section-y">
      <Reveal className="wrap flex flex-col items-center text-center">
        <h2 id="renderers-title" className="display t-section">
          Supported renderers
        </h2>
        <ul className="mt-10 flex flex-wrap items-center justify-center gap-x-16 gap-y-8 sm:mt-12">
          {renderers.map((r) => (
            <li key={r.name}>
              {r.logo ? (
                // eslint-disable-next-line @next/next/no-img-element -- static export: images served as they are
                <img src={r.logo} alt={r.name} className="h-10 w-auto sm:h-12" />
              ) : (
                <span className="display flex h-10 items-center text-[28px] font-semibold tracking-tight text-gl-ink sm:h-12 sm:text-[34px]">
                  {r.name}
                </span>
              )}
            </li>
          ))}
        </ul>
        <p className="mt-10 text-[15px] text-gl-ink-2 sm:mt-12">
          V-Ray: native V-Ray scenes, rendered in V-Ray Standalone and V-Ray for Blender (V-Ray for Maya
          2024+ experimental). Redshift and others: through Alembic and USD exports.{' '}
          <Link href="/docs/other-renderers" className="font-medium text-gl-ink underline underline-offset-4">
            See how
          </Link>
        </p>
      </Reveal>
    </section>
  );
}

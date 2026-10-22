// SPDX-License-Identifier: Apache-2.0
import Link from 'next/link';
import { siteConfig } from '@/site.config';

const columns = [
  {
    title: 'Learn',
    links: [
      { href: '/docs', label: 'Docs' },
      { href: '/docs/installation', label: 'Install' },
      { href: '/docs/quick-start', label: 'Quick start' },
    ],
  },
  {
    title: 'Get GroomLab',
    links: [
      { href: siteConfig.downloadUrl, label: 'Download free' },
      { href: siteConfig.donateUrl, label: 'Support on Ko-fi' },
      { href: siteConfig.repoUrl, label: 'Source on GitHub' },
    ],
  },
  {
    title: 'Help',
    links: [
      { href: siteConfig.discordUrl, label: 'Discord' },
      { href: `mailto:${siteConfig.supportEmail}`, label: 'Support' },
      { href: '/press', label: 'Press kit' },
    ],
  },
  {
    title: 'Legal',
    links: [
      { href: '/terms', label: 'Terms' },
      { href: '/privacy', label: 'Privacy' },
    ],
  },
];

export function SiteFooter() {
  const year = new Date().getFullYear();
  return (
    <footer className="relative border-t border-black/[0.06] bg-gl-mist text-xs text-gl-ink-2">
      <div className="wrap">
        <p className="border-b border-gl-line py-4 leading-relaxed">
          GroomLab needs {siteConfig.requirements.maya} on {siteConfig.requirements.os}. Arnold renders need{' '}
          {siteConfig.requirements.arnold}.
        </p>
        <nav aria-label="Footer" className="grid grid-cols-2 gap-8 py-8 sm:grid-cols-4">
          {columns.map((col) => (
            <div key={col.title}>
              <p className="font-semibold text-gl-ink">{col.title}</p>
              <ul className="mt-2.5 space-y-2">
                {col.links.map((link) => (
                  <li key={link.label}>
                    {link.href.startsWith('/') ? (
                      <Link href={link.href} className="transition-colors hover:text-gl-ink hover:underline">
                        {link.label}
                      </Link>
                    ) : (
                      <a href={link.href} className="transition-colors hover:text-gl-ink hover:underline">
                        {link.label}
                      </a>
                    )}
                  </li>
                ))}
              </ul>
            </div>
          ))}
        </nav>
        <div className="flex flex-col gap-2 border-t border-gl-line py-5 sm:flex-row sm:justify-between">
          <p>
            Copyright © {year} {siteConfig.name}. Version {siteConfig.version}.
          </p>
          <p>Maya, Arnold, Unreal Engine and Unity are trademarks of their respective owners.</p>
        </div>
      </div>
    </footer>
  );
}

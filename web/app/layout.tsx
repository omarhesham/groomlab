// SPDX-License-Identifier: Apache-2.0
import type { Metadata, Viewport } from 'next';
import { Google_Sans_Flex } from 'next/font/google';
import { Provider } from '@/components/provider';
import { siteConfig } from '@/site.config';
import './global.css';

// Google Sans Flex (SIL Open Font License), self-hosted by next/font. Only the optical-size axis
// beside weight: big headlines get the tighter display cut, body text the text cut.
const sans = Google_Sans_Flex({
  subsets: ['latin'],
  variable: '--font-site',
  axes: ['opsz'],
  // Next has no fallback metrics for this family; the system stack in global.css covers the swap.
  adjustFontFallback: false,
});

export const metadata: Metadata = {
  metadataBase: new URL(siteConfig.url),
  title: {
    default: `${siteConfig.name}: ${siteConfig.tagline}`,
    template: `%s | ${siteConfig.name}`,
  },
  description: siteConfig.description,
  openGraph: {
    title: siteConfig.name,
    description: siteConfig.description,
    siteName: siteConfig.name,
    type: 'website',
  },
};

export const viewport: Viewport = {
  themeColor: '#ffffff',
  colorScheme: 'light dark',
};

export default function Layout({ children }: LayoutProps<'/'>) {
  return (
    <html lang="en" className={sans.variable} suppressHydrationWarning>
      <body className="flex min-h-screen flex-col font-sans antialiased">
        <Provider>{children}</Provider>
      </body>
    </html>
  );
}

// SPDX-License-Identifier: Apache-2.0
import type { Metadata, Viewport } from 'next';
import { Inter } from 'next/font/google';
import { Provider } from '@/components/provider';
import { siteConfig } from '@/site.config';
import './global.css';

// The optical-size axis gives big headlines Inter's tighter Display cut.
const inter = Inter({
  subsets: ['latin'],
  variable: '--font-inter',
  axes: ['opsz'],
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
    <html lang="en" className={inter.variable} suppressHydrationWarning>
      <body className="flex min-h-screen flex-col font-sans antialiased">
        <Provider>{children}</Provider>
      </body>
    </html>
  );
}

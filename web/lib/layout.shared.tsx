// SPDX-License-Identifier: Apache-2.0
import type { BaseLayoutProps } from 'fumadocs-ui/layouts/shared';
import { Logo } from '@/components/logo';

export function baseOptions(): BaseLayoutProps {
  return {
    nav: {
      title: <Logo />,
      url: '/',
    },
    links: [{ type: 'button', text: 'Get GroomLab', url: '/#get', secondary: false }],
  };
}

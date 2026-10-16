// SPDX-License-Identifier: Apache-2.0
import defaultMdxComponents from 'fumadocs-ui/mdx';
import { Step, Steps } from 'fumadocs-ui/components/steps';
import { Tab, Tabs } from 'fumadocs-ui/components/tabs';
import type { MDXComponents } from 'mdx/types';
import { AppShortcuts } from '@/components/app-shortcuts';
import { BenchmarkSetup, BenchmarkTable } from '@/components/benchmark-table';
import { ExampleImage, type ExampleImageProps } from '@/components/example-image';
import { hasExample } from '@/lib/examples';

/**
 * An image from public/examples in a docs page. A missing file shows a placeholder,
 * or nothing at all when the image is `optional`.
 */
function Example({ optional, ...props }: Omit<ExampleImageProps, 'variant'> & { optional?: boolean }) {
  if (optional && !hasExample(props.name)) return null;
  return <ExampleImage sizes="(min-width: 1024px) 760px, 100vw" {...props} variant="docs" />;
}

export function getMDXComponents(components?: MDXComponents) {
  return {
    ...defaultMdxComponents,
    Step,
    Steps,
    Tab,
    Tabs,
    Example,
    AppShortcuts,
    BenchmarkSetup,
    BenchmarkTable,
    ...components,
  } satisfies MDXComponents;
}

export const useMDXComponents = getMDXComponents;

declare global {
  type MDXProvidedComponents = ReturnType<typeof getMDXComponents>;
}

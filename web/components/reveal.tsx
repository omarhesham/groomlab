// SPDX-License-Identifier: Apache-2.0
'use client';
import { useEffect, useRef, type ReactNode } from 'react';

/**
 * Fades its children in the first time they scroll into view. The hidden state
 * lives in global.css and only applies when scripts run and the reader has not
 * asked for reduced motion, so the content is never lost.
 */
export function Reveal({
  children,
  className,
  delay = 0,
}: {
  children: ReactNode;
  className?: string;
  delay?: number;
}) {
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    const observer = new IntersectionObserver(
      (entries) => {
        for (const entry of entries) {
          if (entry.isIntersecting) {
            el.setAttribute('data-shown', '');
            observer.disconnect();
          }
        }
      },
      { rootMargin: '0px 0px -40px 0px', threshold: 0 },
    );
    observer.observe(el);
    return () => observer.disconnect();
  }, []);

  return (
    <div ref={ref} data-reveal="" className={className} style={delay ? { transitionDelay: `${delay}ms` } : undefined}>
      {children}
    </div>
  );
}

// SPDX-License-Identifier: Apache-2.0
import Image from 'next/image';
import type { CSSProperties } from 'react';
import { cn } from '@/lib/cn';
import { getExample } from '@/lib/examples';

export interface ExampleImageProps {
  /** File name inside public/examples, e.g. "hero.png". */
  name: string;
  alt: string;
  /** Shown under the image in docs, and inside the frame when the file is missing. */
  caption?: string;
  /** Fixed frame, e.g. "16 / 9". Without it the image keeps its own proportions. */
  aspect?: string;
  sizes?: string;
  priority?: boolean;
  /** Which part of the image stays in view when a fixed frame crops it. */
  position?: 'center' | 'top' | 'left';
  /** "landing" frames sit on the viewport's charcoal; "docs" follows the docs theme. */
  variant?: 'landing' | 'docs';
  /** Landing only: the image drifts a little inside its frame as the page scrolls. */
  parallax?: boolean;
  /**
   * Landing only: the image is a strip of this many frames side by side, shown
   * one frame at a time as a looping flipbook (the first frame with reduced motion).
   */
  frames?: number;
  className?: string;
}

/**
 * An example render or screenshot in a rounded frame. If the file is not in
 * public/examples at build time, the frame shows a soft gradient with the
 * caption instead of a broken image.
 */
export function ExampleImage({
  name,
  alt,
  caption,
  aspect,
  sizes = '(min-width: 1024px) 60vw, 100vw',
  priority,
  position = 'center',
  variant = 'landing',
  parallax,
  frames,
  className,
}: ExampleImageProps) {
  const info = getExample(name);
  const frameAspect =
    frames && info ? `${info.width / frames} / ${info.height}` : (aspect ?? (info ? undefined : '16 / 9'));
  const style: CSSProperties | undefined = frameAspect ? { aspectRatio: frameAspect } : undefined;

  const frame = cn(
    'relative overflow-hidden',
    variant === 'landing'
      ? 'rounded-[20px] bg-gl-night sm:rounded-[28px]'
      : 'rounded-2xl bg-fd-muted ring-1 ring-fd-border sm:rounded-3xl',
    className,
  );

  const body = !info ? (
    <div style={style} className={frame} role="img" aria-label={alt}>
      {/* Docs show the caption under the frame, so the frame names the image instead. */}
      <Placeholder label={variant === 'docs' ? alt : (caption ?? alt)} variant={variant} />
    </div>
  ) : frames ? (
    <div style={style} className={cn(frame, 'flipbook-frame')}>
      <div
        className="flipbook absolute inset-y-0 left-0 flex"
        style={{ width: `${frames * 100}%`, '--frames': frames } as CSSProperties}
      >
        <Image src={info.src} alt={alt} width={info.width} height={info.height} sizes={sizes} priority={priority} className="h-full w-full" />
      </div>
    </div>
  ) : frameAspect ? (
    <div style={style} className={frame}>
      <Image
        src={info.src}
        alt={alt}
        fill
        sizes={sizes}
        priority={priority}
        className={cn(
          'object-cover',
          position === 'top' && 'object-top',
          position === 'left' && 'object-left',
          parallax && 'parallax',
        )}
      />
    </div>
  ) : (
    <div className={frame}>
      <Image
        src={info.src}
        alt={alt}
        width={info.width}
        height={info.height}
        sizes={sizes}
        priority={priority}
        className={cn('h-auto w-full', parallax && 'parallax')}
      />
    </div>
  );

  if (variant === 'docs') {
    return (
      <figure className="not-prose my-6">
        {body}
        {caption && (
          <figcaption className="mt-2 text-center text-sm text-fd-muted-foreground">{caption}</figcaption>
        )}
      </figure>
    );
  }
  return body;
}

function Placeholder({ label, variant }: { label: string; variant: 'landing' | 'docs' }) {
  return (
    <div className="absolute inset-0 flex items-center justify-center">
      <div
        aria-hidden="true"
        className="absolute inset-0 opacity-70"
        style={{
          background:
            'radial-gradient(60% 70% at 15% 20%, rgba(47,123,246,0.35), transparent 70%),' +
            'radial-gradient(50% 60% at 85% 30%, rgba(163,107,255,0.28), transparent 70%),' +
            'radial-gradient(55% 60% at 60% 95%, rgba(255,197,61,0.22), transparent 70%)',
        }}
      />
      <svg
        aria-hidden="true"
        viewBox="0 0 400 200"
        preserveAspectRatio="none"
        className="absolute inset-0 size-full opacity-20"
        fill="none"
      >
        {Array.from({ length: 9 }, (_, i) => (
          <path
            key={i}
            d={`M${-20 + i * 50} 210 C ${20 + i * 50} 120, ${-10 + i * 50} 70, ${60 + i * 50} -10`}
            stroke={['#2F7BF6', '#FFC53D', '#2ECC8F', '#FF4D4F', '#A36BFF'][i % 5]}
            strokeWidth="2"
            strokeLinecap="round"
          />
        ))}
      </svg>
      <p
        className={cn(
          'relative px-6 text-center text-sm font-medium',
          variant === 'landing' ? 'text-gl-white/80' : 'text-fd-muted-foreground',
        )}
      >
        {label}
      </p>
    </div>
  );
}

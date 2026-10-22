// SPDX-License-Identifier: Apache-2.0

/**
 * The site's buttons: softly rounded, with squash and stretch (see `.squish` in global.css). For
 * the dark pages: the blue fill (#2468e0, AA under white text), a hairline outline, and a quiet fill.
 */
export const button = {
  base: 'squish inline-flex items-center justify-center gap-2 rounded-[10px] font-medium focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-gl-blue',
  md: 'h-11 px-5 text-[0.95rem]',
  lg: 'h-[3.25rem] px-7 text-[1.0625rem]',
  primary:
    'bg-gl-blue-strong text-white shadow-[0_12px_30px_-12px_rgba(47,123,246,0.8)] hover:bg-[#1d5bcc] hover:shadow-[0_16px_34px_-12px_rgba(47,123,246,0.85)]',
  outline: 'text-gl-ink ring-1 ring-white/25 ring-inset hover:bg-white/[0.06] hover:ring-white/45',
  quiet: 'bg-white/[0.07] text-gl-ink ring-1 ring-white/10 ring-inset hover:bg-white/[0.11]',
};

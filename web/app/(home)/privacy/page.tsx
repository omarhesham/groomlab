// SPDX-License-Identifier: Apache-2.0
// STARTER TEXT, NOT LEGAL ADVICE: a plain starting point describing what GroomLab and this site
// actually collect. Have a lawyer review it (and add the legal entity and, if needed, an EU/UK
// representative) before relying on it. Keep it true: update it whenever the apps or the site start
// sending anything new.

import type { Metadata } from 'next';
import { PolicyPage, PolicySection } from '@/components/policy-page';
import { siteConfig } from '@/site.config';

export const metadata: Metadata = {
  title: 'Privacy Policy',
  description: 'What GroomLab and getgroomlab.com collect, and why.',
};

export default function PrivacyPage() {
  const { supportEmail, donateUrl } = siteConfig;
  return (
    <PolicyPage
      title="Privacy Policy"
      updated="October 13, 2026"
      intro={
        <p>
          We collect as little as we can. GroomLab has no accounts and no license keys, never uploads your grooms,
          scenes or renders, and collects no usage analytics.
        </p>
      }
    >
      <PolicySection title="When GroomLab checks for updates">
        <p>
          GroomLab can check whether a newer version exists. That request reads a public file from our download server
          and sends nothing about you or your machine beyond what any web request carries: your IP address, and
          GroomLab&apos;s version as the user agent. Nothing else goes online; render nodes and the command line tools
          never do.
        </p>
      </PolicySection>

      <PolicySection title="Donations">
        <p>
          Donations go through <a href={donateUrl}>Ko-fi</a>, which collects what a payment needs and processes it under
          its own privacy policy. We never see your card details. We may see your name, email address and message if
          you choose to share them with your donation.
        </p>
      </PolicySection>

      <PolicySection title="This website">
        <p>
          getgroomlab.com uses no tracking cookies and no advertising. Our hosting and download providers may keep
          standard server logs (IP address, pages requested, browser) for security and to keep the site running.
        </p>
      </PolicySection>

      <PolicySection title="Email">
        <p>
          If you write to us, we keep the conversation to help you and for our records. We don&apos;t send marketing
          email unless you asked for it.
        </p>
      </PolicySection>

      <PolicySection title="Your rights">
        <p>
          You can ask us for a copy of the data we hold about you, or ask us to correct or delete it, at{' '}
          <a href={`mailto:${supportEmail}`}>{supportEmail}</a>. We don&apos;t sell personal data.
        </p>
      </PolicySection>

      <PolicySection title="Changes">
        <p>We will update this page when what we collect changes; the date above says when it last did.</p>
      </PolicySection>
    </PolicyPage>
  );
}

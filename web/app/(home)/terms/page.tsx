// SPDX-License-Identifier: Apache-2.0
// STARTER TEXT, NOT LEGAL ADVICE: plain terms for free, open-source software funded by donations.
// The software's licenses (Apache-2.0; the Blender add-on GPL-3.0-or-later) govern copying and
// changing GroomLab; these terms keep the rest.
// Have a lawyer review it (and fill in the legal entity, address and governing law) before relying on it.

import type { Metadata } from 'next';
import { PolicyPage, PolicySection } from '@/components/policy-page';
import { siteConfig } from '@/site.config';

export const metadata: Metadata = {
  title: 'Terms of Use',
  description: 'The terms for using GroomLab, free for everyone.',
};

export default function TermsPage() {
  const { supportEmail, donateUrl } = siteConfig;
  return (
    <PolicyPage
      title="Terms of Use"
      updated="October 15, 2026"
      intro={
        <p>
          These terms cover downloading and using GroomLab: the Maya plugin, the GroomLab app, the Blender add-on and
          helper, and the command line and rendering tools. By installing GroomLab you agree to them. Questions:{' '}
          <a href={`mailto:${supportEmail}`}>{supportEmail}</a>.
        </p>
      }
    >
      <PolicySection title="Free for everyone">
        <p>
          GroomLab is free. Anyone may download it and use it, on any number of machines, for any purpose, personal or
          commercial: every feature and every export, with no key and no watermark. Render nodes, the procedurals, the
          command line tools, the C API and the Python module are free on any number of machines too.
        </p>
        <p>
          GroomLab is funded by donations. If it helps you, you can support it on <a href={donateUrl}>Ko-fi</a>.
          Donations are voluntary and don&apos;t change what GroomLab does for you.
        </p>
      </PolicySection>

      <PolicySection title="Open source">
        <p>
          GroomLab is open source. Its source code is licensed under the{' '}
          <a href="https://www.apache.org/licenses/LICENSE-2.0">Apache License 2.0</a>, and the Blender add-on under
          the <a href="https://www.gnu.org/licenses/gpl-3.0.html">GNU GPL, version 3 or later</a>. Those licenses,
          not these terms, say how you may copy, change and redistribute GroomLab. The components GroomLab includes
          keep their own licenses (THIRD_PARTY_NOTICES.txt in every download).
        </p>
        <p>
          The GroomLab name and logo identify the official releases. If you distribute a changed version, please
          give it a name of your own so nobody mistakes it for an official build.
        </p>
      </PolicySection>

      <PolicySection title="Your work">
        <p>
          Grooms, scenes, renders and exports you make with GroomLab belong to you. We don&apos;t claim any rights in
          them, and GroomLab does not upload them anywhere.
        </p>
      </PolicySection>

      <PolicySection title="Updates and changes">
        <p>
          We may update GroomLab and change or stop offering features. We may change these terms; the date above says
          when they last changed.
        </p>
      </PolicySection>

      <PolicySection title="No warranty and limits of liability">
        <p>
          GroomLab is provided &quot;as is&quot;, free of charge. We work to make it reliable, but we can&apos;t
          promise it will be free of errors or fit every purpose; keep backups of your work. To the extent the law
          allows, we are not liable for any loss arising from using GroomLab, including indirect or consequential
          losses such as lost profits or data. Nothing here limits rights you have that can&apos;t be limited by
          contract.
        </p>
      </PolicySection>

      <PolicySection title="Contact">
        <p>
          <a href={`mailto:${supportEmail}`}>{supportEmail}</a>
        </p>
      </PolicySection>
    </PolicyPage>
  );
}

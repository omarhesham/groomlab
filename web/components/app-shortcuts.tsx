// SPDX-License-Identifier: Apache-2.0
import fs from 'node:fs';
import path from 'node:path';

/**
 * The standalone app's keyboard shortcuts, read at build time from the table its own
 * Help > Keyboard Shortcuts page shows (MainWindow::showHelp in hosts/app/src/main_window.cpp),
 * so the docs and the app never disagree. Each row there is one line of the form
 * `"<tr><td><b>Keys</b> (where)</td><td>What it does</td></tr>"`. The build fails when the file
 * is missing or no row parses, rather than shipping a stale or empty table.
 */
const source = path.join(process.cwd(), '..', 'hosts', 'app', 'src', 'main_window.cpp');
const row = /<tr><td><b>(.+?)<\/b>(.*?)<\/td><td>(.+?)<\/td><\/tr>/g;

interface Shortcut {
  keys: string;
  where: string;
  does: string;
}

function readShortcuts(): Shortcut[] {
  let text: string;
  try {
    text = fs.readFileSync(source, 'utf8');
  } catch {
    throw new Error(`AppShortcuts: cannot read ${source} (the app's Keyboard Shortcuts page)`);
  }
  const plain = (html: string) =>
    html.replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&amp;/g, '&').trim();
  const rows = [...text.matchAll(row)].map((m) => ({ keys: plain(m[1]), where: plain(m[2]), does: plain(m[3]) }));
  if (rows.length === 0) throw new Error(`AppShortcuts: no shortcut rows found in ${source}`);
  return rows;
}

export function AppShortcuts() {
  const shortcuts = readShortcuts();
  return (
    <table>
      <thead>
        <tr>
          <th>Keys</th>
          <th>What it does</th>
        </tr>
      </thead>
      <tbody>
        {shortcuts.map((s) => (
          <tr key={s.keys + s.where}>
            <td>
              <strong>{s.keys}</strong>
              {s.where && <> {s.where}</>}
            </td>
            <td>{s.does}</td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}

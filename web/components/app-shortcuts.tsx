// SPDX-License-Identifier: Apache-2.0
import fs from 'node:fs';
import path from 'node:path';

/**
 * The standalone app's keyboard shortcuts, read at build time from the one shortcut table every
 * host shows (engine/core/src/shortcuts.cpp: the app's Help > Keyboard Shortcuts page is built from
 * it too), so the docs and the app never disagree. Each row there is one line of the form
 * `{"id", "Context", "keys", "action", hosts[, kSet]},` where hosts is an expression of kMaya,
 * kApp, kBlender and kAll; the rows whose hosts name kApp or kAll are the app's. The build fails
 * when the file is missing or no row parses, rather than shipping a stale or empty table.
 */
const source = path.join(process.cwd(), '..', 'engine', 'core', 'src', 'shortcuts.cpp');
const str = '"((?:[^"\\\\]|\\\\.)*)"';
const row = new RegExp(`^\\s*\\{${str},\\s*${str},\\s*${str},\\s*${str},\\s*([^,}]+?)\\s*(?:,\\s*kSet\\s*)?\\},?\\s*$`, 'gm');

interface Shortcut {
  context: string;
  keys: string;
  does: string;
}

function readShortcuts(): Shortcut[] {
  let text: string;
  try {
    text = fs.readFileSync(source, 'utf8');
  } catch {
    throw new Error(`AppShortcuts: cannot read ${source} (the shortcut table)`);
  }
  const plain = (s: string) => s.replace(/\\(.)/g, '$1').trim();
  const rows = [...text.matchAll(row)]
    .filter((m) => /\bk(App|All)\b/.test(m[5]))
    .map((m) => ({ context: plain(m[2]), keys: plain(m[3]), does: plain(m[4]) }));
  if (rows.length === 0) throw new Error(`AppShortcuts: no app shortcut rows found in ${source}`);
  return rows;
}

export function AppShortcuts() {
  const shortcuts = readShortcuts();
  // Grouped by context, in the table's order (File, Modes, Brush, Viewport, Graph).
  const contexts = [...new Set(shortcuts.map((s) => s.context))];
  return (
    <table>
      <thead>
        <tr>
          <th>Keys</th>
          <th>What it does</th>
        </tr>
      </thead>
      <tbody>
        {contexts.map((context) => [
          <tr key={`context-${context}`}>
            <td colSpan={2}>
              <em>{context}</em>
            </td>
          </tr>,
          ...shortcuts
            .filter((s) => s.context === context)
            .map((s) => (
              <tr key={`${context}-${s.keys}-${s.does}`}>
                <td>
                  <strong>{s.keys}</strong>
                </td>
                <td>{s.does}</td>
              </tr>
            )),
        ])}
      </tbody>
    </table>
  );
}

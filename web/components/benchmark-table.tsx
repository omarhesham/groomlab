// SPDX-License-Identifier: Apache-2.0
import { benchmarkInfo, benchmarkRows, factor, formatFactor, formatValue, type Count } from '@/lib/benchmark';

/** The machine and setup the numbers come from. */
export function BenchmarkSetup() {
  return (
    <ul>
      <li>
        <strong>Machine:</strong> {benchmarkInfo.machine}; {benchmarkInfo.host}.
      </li>
      <li>
        <strong>Setup:</strong> {benchmarkInfo.setup}
      </li>
      <li>
        <strong>Runs:</strong> {benchmarkInfo.runs} XGen measured {benchmarkInfo.xgenRun}, GroomLab{' '}
        {benchmarkInfo.groomlabRun} (build {benchmarkInfo.groomlabVersion}).
      </li>
    </ul>
  );
}

/** Every benchmark measure at one strand count, for the Performance docs page. */
export function BenchmarkTable({ count }: { count: Count }) {
  const dash = <span className="text-fd-muted-foreground">–</span>;
  const cell = (value: number | undefined, unit: Parameters<typeof formatValue>[1]) =>
    value === undefined ? dash : formatValue(value, unit);
  return (
    <table>
      <thead>
        <tr>
          <th>Measure</th>
          <th>XGen</th>
          <th>GroomLab</th>
          <th>Follow Scalp</th>
          <th>No Curl</th>
          <th>GPU preview</th>
          <th>GroomLab vs XGen</th>
        </tr>
      </thead>
      <tbody>
        {benchmarkRows.map((row) => {
          const v = row.values[count];
          const f = factor(row, count);
          return (
            <tr key={row.id}>
              <td>
                {row.label}
                {row.higherIsBetter ? ' (higher is better)' : ''}
              </td>
              <td>{cell(v.xgen, row.unit)}</td>
              <td>{cell(v.groomlab, row.unit)}</td>
              <td>{cell(v.follow, row.unit)}</td>
              <td>{cell(v.nocurl, row.unit)}</td>
              <td>{cell(v.gpu, row.unit)}</td>
              <td>
                {f === undefined
                  ? dash
                  : f > 0.95 && f < 1.05
                    ? 'About even'
                    : f >= 1
                    ? `${formatFactor(f)} ${row.better ?? 'better'}${row.compare === 'follow' ? ' (Follow Scalp)' : row.compare === 'gpu' ? ' (GPU preview)' : ''}`
                    : `XGen ${formatFactor(1 / f)} ${row.better ?? 'better'}`}
              </td>
            </tr>
          );
        })}
      </tbody>
    </table>
  );
}

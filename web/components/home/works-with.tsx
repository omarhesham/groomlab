// SPDX-License-Identifier: Apache-2.0
const items = ['Maya 2022–2027', 'Arnold', 'Unreal Engine', 'Unity', 'glTF', 'Alembic', 'USD'];

export function WorksWith() {
  return (
    <div className="mt-10 flex flex-col items-center gap-3 text-center text-sm sm:mt-12 sm:flex-row sm:justify-center sm:gap-5 sm:text-[15px]">
      <h2 className="font-semibold text-gl-ink">Works with</h2>
      <ul className="flex flex-wrap justify-center gap-x-5 gap-y-1.5 text-gl-ink-2">
        {items.map((item) => (
          <li key={item}>{item}</li>
        ))}
      </ul>
    </div>
  );
}

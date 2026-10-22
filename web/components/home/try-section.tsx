// SPDX-License-Identifier: Apache-2.0
import { SectionHead } from './section-node';
import { TryGraph } from './try-graph';

/** The hands-on moment: a few real nodes and the strands they grow. */
export function TrySection() {
  return (
    <section id="try" aria-labelledby="try-title" className="section-y pt-8! sm:pt-12!">
      <div className="wrap">
        <SectionHead
          id="try-title"
          cat="Shape"
          index="01"
          tag="Node graph"
          title="Style with a node graph."
          muted="Try it."
          lede={
            <p>
              Clumps, curls, frizz and color are nodes, and in GroomLab sliders update 100k strands live. This one is
              small: bypass a node or drag a slider and watch the strands follow.
            </p>
          }
        />
        <TryGraph />
      </div>
    </section>
  );
}

# GroomLab 1.0 pilot

The 1.0 gate is **all P0 requirements done and a pilot sign-off**. The code side is done: every
P0 requirement is built and covered by automated tests. What remains needs people: artists
grooming real assets and reporting what gets in their way.

## Scope

| | |
| --- | --- |
| Who | 2–3 groomers, 1 CFX artist, 1 lighter (or one person covering several roles) |
| What | One hero groom (long hair, character head) and one fur groom (creature or animal) |
| Where | Maya 2022, Arnold (MtoA), Windows; one farm render from a groom file |
| Setup | The GroomLab module installed from a tagged build; the [user guide](USER_GUIDE.md) |

## What each workflow must do

Every row is a pass/fail check. A row passes when the pilot artist does it on the pilot asset
without help from a developer.

| # | Workflow | Pass when |
| --- | --- | --- |
| 1 | Start a groom | Groom created on the asset's scalp; strands visible and interactive at preview density |
| 2 | Hierarchy | Primaries placed and brushed, two child levels generated, one side mirrored with symmetry kept |
| 3 | Look dev | Clumps (procedural and painted), noise and curl, one texture or painted map, and one compound reused through a preset |
| 4 | Existing curves | A groom started from Maya curves or an Alembic guide file |
| 5 | Simulation | Primaries simulated (nHair, or an external cache); children follow; the groom file renders the simulated groom |
| 6 | Render | Scene render in Arnold with motion blur; a farm render of the same shot from a groom file; a USD or Alembic cache loaded in another package |
| 7 | Scene handling | The groom referenced into a shot scene with a namespace, and the project moved, with nothing broken |

## What to record

For each artist and asset:

- **Time:** how long each workflow took, and where time was lost.
- **Problems:** every error message seen, with `groomlab.errors(groom)` output and the scene if
  possible.
- **Crashes:** Maya crashes. Any crash is a blocker.
- **Gaps:** what they reached for and could not find, compared with the Tonic, Yeti or XGen
  tools they know.
- **Speed:** their judgement of interactive speed on their machine, next to the budgets the tests
  measure (see the README).

## Sign-off

| Criterion | Target |
| --- | --- |
| Workflow checks | All rows pass for the hero groom; rows 1–3 and 6 pass for the fur groom |
| Crashes | None open |
| Data loss | None: scenes and groom files always reopen as saved |
| Blocking issues | None open; issues found are triaged into 1.0 fixes or post-1.0 work |
| Sign-off | The pilot artists and the pipeline owner agree it is ready for production |

## Known limits going in

These are known and documented, so the pilot can confirm they're acceptable or push them into
1.0:

- **Symmetry:** it is across a world axis, not topological. An asymmetric or posed head needs
  mirroring before posing.
- **Texture formats:** texture maps read PNG, JPEG, TGA, BMP, PSD, GIF, HDR and PNM; EXR and TIFF
  are not read yet.
- **Maya version:** only Maya 2022 has been built and tested.
- **Viewport shading:** lit strands are a look check, not the render. Color modes other than
  `solid` are flat, and ribbons in them use a fixed key light.
- **Coverage:** core line coverage is reported by CI, and the 80% target is not yet enforced as a
  gate.

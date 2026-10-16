# SPDX-License-Identifier: Apache-2.0
"""Re-record what every shipped preset makes (tests/data/golden/PresetLooks.json, PresetDigests.json).

The core test PresetLooks.EveryShippedPresetMakesWhatItMadeBefore evaluates each preset on the
strokes fixture's groom and compares a digest (counts, IDs, point sums and bounds, widths,
channels) against the recording. After an intended change of what a preset makes (a default, an
operator's behaviour, the preset itself), run this and review the JSON diff. It records each
preset's exact bits too (Determinism.EveryPresetMakesTheRecordedBitsOnEveryPlatform,
PresetDigests.json): record on Windows (MSVC's math; or anywhere with GROOMLAB_PORTABLE_MATH on).

    python tools/dev/record_preset_looks.py                 # the ninja-maya2022 build
    python tools/dev/record_preset_looks.py --build build/win-maya2022
"""

import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", default=os.path.join(ROOT, "build", "ninja-maya2022"))
    args = parser.parse_args()
    exe = None
    for folder, _, files in os.walk(os.path.join(args.build, "tests")):
        if "groomlab_tests.exe" in files or "groomlab_tests" in files:
            exe = os.path.join(folder, "groomlab_tests.exe" if "groomlab_tests.exe" in files else "groomlab_tests")
            break
    if not exe:
        sys.exit("no groomlab_tests under %s: build first" % args.build)
    env = dict(os.environ, GROOMLAB_UPDATE_GOLDEN="1")
    code = subprocess.call([exe, "--gtest_filter=PresetLooks.*:Determinism.EveryPresetMakesTheRecordedBitsOnEveryPlatform"],
                           env=env)
    print("recorded tests/data/golden/PresetLooks.json and PresetDigests.json: review `git diff tests/data/golden`")
    return code


if __name__ == "__main__":
    sys.exit(main())

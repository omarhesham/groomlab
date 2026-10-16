// SPDX-License-Identifier: Apache-2.0
#pragma once

// When Maya is about to go down without a crash report (abort(), std::terminate,
// a pure virtual call, an invalid CRT parameter: Maya exits with code 3 and no
// MayaCrashLog), write the call stack to %TEMP%/GroomLabCrash_<time>.log first,
// with GroomLab's frames named from its PDB. Installed while the plugin is loaded.

namespace groomlab::maya {

void installCrashLog();
void removeCrashLog();

}  // namespace groomlab::maya

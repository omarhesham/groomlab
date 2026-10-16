// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace groomlab {

class OperatorRegistry;

namespace op {
constexpr const char* kImportCurves = "ImportCurves";  // [Mesh] -> Guides, from an Alembic file
}  // namespace op

// Registers operators that need Alembic. The core cannot depend on Alembic,
// so hosts that link groomlab_alembic call this once at startup (the Maya
// plugin, the Arnold translator, the CLI). Safe to call more than once.
void registerAlembicOperators(OperatorRegistry& registry);

}  // namespace groomlab

// SPDX-License-Identifier: Apache-2.0
#pragma once

// Scalps from mesh files for a new groom (the MCP server's new_groom): OBJ (vertices, texture
// coordinates, polygons triangulated as fans) and Alembic (the first polygon mesh, or the one
// named, at its first sample). The standalone app reads OBJ the same way (hosts/app mesh_io.h).

#include <memory>
#include <stdexcept>
#include <string>

#include "groomlab/scalp_mesh.h"

namespace groomlab::cli {

class MeshFileError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// `v`, `vt` and `f` lines (`v/vt`, `v//vn`, `v/vt/vn`, negative indices from the end); UVs per
// corner when every face has them. Throws MeshFileError saying the line.
std::shared_ptr<const ScalpMesh> parseObjMesh(const std::string& text, const std::string& label);

// By extension: .obj or .abc (`object`: the Alembic mesh's name, "" the first). Throws
// MeshFileError, also for a mesh without triangles.
std::shared_ptr<const ScalpMesh> readMeshFile(const std::string& path, const std::string& object = {});

}  // namespace groomlab::cli

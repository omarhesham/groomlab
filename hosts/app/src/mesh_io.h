// SPDX-License-Identifier: Apache-2.0
#pragma once

// Scalps from mesh files, for a new groom made on a mesh: a small OBJ reader (vertices, texture
// coordinates, faces triangulated as fans) and Alembic through the core's AlembicMeshReader.
// The same OBJ reader also gives a Surface to render beside the groom (a scanned head): its
// normals per corner and the textures its material library names.

#include <memory>
#include <stdexcept>
#include <string>

#include "groomlab/scalp_mesh.h"
#include "surface.h"

namespace groomlab::app {

class MeshFileError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// `v`, `vt` and `f` lines (`v/vt`, `v//vn`, `v/vt/vn`, negative indices from the end), every
// polygon triangulated as a fan from its first corner, UVs per corner when every face has them.
// `group`: the faces under that `g` (or `o`) line alone, the points they use (the wolf sample's
// body without its eyes). Throws MeshFileError.
std::shared_ptr<const ScalpMesh> readObjMesh(const std::string& path, const std::string& group = {});
// The same from OBJ text (tests).
std::shared_ptr<const ScalpMesh> parseObjMesh(const std::string& text, const std::string& label = "obj", const std::string& group = {});

// The OBJ as a surface to render: the mesh as above, `vn` normals per corner when every face
// has them, and the material of its first `usemtl` from the `mtllib` beside it (the maps:
// map_Kd the albedo, map_Bump or bump the normal map, map_Ks the specular map, map_Pr
// roughness, map_Pm metallic, map_Ka occlusion; Kd the base color), the map paths resolved
// against the file's folder, over `material` (a preset: the library's maps and its Kd, when
// it has one, replace the preset's; a scan's albedo map is used as it is under Kd 1 1 1).
// Without a library the surface keeps `material` as given. `exceptGroup`: every face but that
// group's (the rest of a sample beside its scalp). Throws MeshFileError.
Surface readObjSurface(const std::string& path, const SurfaceMaterial& material = SurfaceMaterial(), const std::string& exceptGroup = {});
// The same from OBJ text, with `mtlText` standing in for the library (tests).
Surface parseObjSurface(const std::string& text, const std::string& mtlText, const std::string& label = "obj",
                        const std::string& folder = {}, const SurfaceMaterial& material = SurfaceMaterial(), const std::string& exceptGroup = {});

// The first polygon mesh of an Alembic archive (or the one named `object`) at `seconds`, with
// its faces and UVs. Throws MeshFileError.
std::shared_ptr<const ScalpMesh> readAlembicMesh(const std::string& path, const std::string& object = {},
                                                 double seconds = 0.0);

// By extension: .obj or .abc. Throws MeshFileError for another.
std::shared_ptr<const ScalpMesh> readMeshFile(const std::string& path);

}  // namespace groomlab::app

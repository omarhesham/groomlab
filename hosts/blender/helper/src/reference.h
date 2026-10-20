// SPDX-License-Identifier: Apache-2.0
#pragma once

// Grooming from photos (docs/BLENDER.md "Photos"): Maya's Groom Tools > Photos (groomlab.reference:
// a photo as a camera lined up with the mesh by pairs of points) and Guides from Photos
// (groomlab.from_reference), over the helper's session. The photos are kept with the groom (its
// file's "referencePhotos", the standalone app's Reference window's: each photo's file, size,
// solved camera and landmark pairs), in groom space; the host sends and reads them in its own.

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

namespace groomlab::helper {

struct Session;

// reference_photos, set_reference_photos, solve_camera, guides_from_photos.
bool isReferenceCommand(const std::string& cmd);
// Runs one; `session` gives the request's session.
nlohmann::json runReferenceCommand(const std::string& cmd, const nlohmann::json& args, const std::function<Session&()>& session);

}  // namespace groomlab::helper

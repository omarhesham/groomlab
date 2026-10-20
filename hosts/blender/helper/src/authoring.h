// SPDX-License-Identifier: Apache-2.0
#pragma once

// The groom's authoring beyond the brushes (docs/BLENDER.md "Authoring"): the graph's own edits
// as Maya's Graph Editor makes them (Group and Ungroup, exposed params, a node put on a link,
// sliders and keyed params, each one undo step on the session's stack), the controls' values,
// the node library (Save as Node, script nodes, Save as Preset), and what brushes edit: the base
// groom, a corrective layer (shot sculpting) or a Sculpt node, with the corrective stack's edits.

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

namespace groomlab::helper {

struct Session;

// edit_graph's undoable ops (group, ungroup, expose_param...): true when `op` is one of them,
// its answer in `result`. Throws RequestError or GraphError (the graph unchanged).
bool runGraphAuthoring(Session& s, const std::string& op, const nlohmann::json& args, nlohmann::json& result);

// The other authoring commands (set_controls, save_node, correctives_list, sculpt_target...).
bool isAuthoringCommand(const std::string& cmd);
// Runs one; `session` gives the request's session (the commands that take one call it).
nlohmann::json runAuthoringCommand(const std::string& cmd, const nlohmann::json& args, const std::function<Session&()>& session);

}  // namespace groomlab::helper

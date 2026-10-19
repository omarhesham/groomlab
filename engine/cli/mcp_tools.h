// SPDX-License-Identifier: Apache-2.0
#pragma once

// The MCP server's tools (mcp_server.h speaks the protocol): what an AI agent can do with
// GroomLab. Grooms are held in memory by name (a scalp, a guide hierarchy and a graph: what a
// .groom file holds), each with an undo stack; every tool that changes one answers with a short
// summary of the groom after the change (strands, points, errors). Files are read and written only
// under the root the server was started with; GroomLab's own presets, nodes, sample meshes and
// docs are read where they ship.

#include <memory>
#include <string>

#include <nlohmann/json.hpp>

namespace groomlab::cli {

struct McpSettings {
    std::string root;    // files are read and written only under this folder (absolute)
    std::string exeDir;  // the executable's folder: the shipped presets, nodes, meshes and docs beside it
};

// A tool's answer: MCP content blocks (text, image), and whether it is an error (the agent reads
// the message and tries again; a protocol error is something else).
struct ToolResult {
    nlohmann::json content = nlohmann::json::array();
    bool isError = false;
};

class McpTools {
public:
    explicit McpTools(McpSettings settings);
    ~McpTools();
    McpTools(const McpTools&) = delete;
    McpTools& operator=(const McpTools&) = delete;

    // tools/list's "tools": each tool's name, title, description and input JSON Schema.
    nlohmann::json list() const;
    bool has(const std::string& name) const;
    // Runs a tool. Never throws for what the arguments or the groom get wrong (an error result).
    ToolResult call(const std::string& name, const nlohmann::json& arguments);

    const std::string& root() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace groomlab::cli

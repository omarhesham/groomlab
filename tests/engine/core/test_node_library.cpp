// SPDX-License-Identifier: Apache-2.0
// Compound nodes (node_library.h): .glnode files, the library folders, grooms carrying their copy.

#include <gtest/gtest.h>

#include <cstdlib>
#include <functional>
#include <map>
#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "groomlab/graph_edit.h"
#include "groomlab/graph_io.h"
#include "groomlab/groom.h"
#include "groomlab/node_library.h"
#include "test_helpers.h"


namespace groomlab {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

void setEnv(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    if (value.empty()) unsetenv(name);
    else setenv(name, value.c_str(), 1);
#endif
}

// A fresh folder per test; the library reads only it (and an empty user folder).
struct Library {
    fs::path root;
    explicit Library(const std::string& name) {
        root = fs::temp_directory_path() / ("groomlab_nodes_" + name);
        fs::remove_all(root);
        fs::create_directories(root / "a");
        fs::create_directories(root / "b");
        fs::create_directories(root / "user");
        setEnv("GROOMLAB_USER_NODES", (root / "user").generic_string());
        setEnv("GROOMLAB_NODE_PATH", "");
        setShippedNodeDir("");
        refreshNodeLibrary();
    }
    ~Library() {
        setEnv("GROOMLAB_USER_NODES", "");
        setEnv("GROOMLAB_NODE_PATH", "");
        refreshNodeLibrary();
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    std::string dir(const char* sub) const { return (root / sub).generic_string(); }
    void write(const char* sub, const std::string& file, const std::string& text) const {
        std::ofstream(root / sub / file, std::ios::binary) << text;
        refreshNodeLibrary();
    }
};

const char* kBase = R"j({"version": 1, "output": "out",
  "nodes": [{"name": "scalp", "type": "Scalp"},
            {"name": "scatter", "type": "Scatter", "params": {"density": 200.0}},
            {"name": "out", "type": "Output"}],
  "connections": [{"from": "scalp", "to": "scatter", "input": "scalp"}]})j";

// The scatter straight into the output.
std::string plainGraph() {
    json g = json::parse(kBase);
    g["connections"].push_back({{"from", "scatter"}, {"to", "out"}, {"input", "strands"}});
    return g.dump();
}

// scatter -> frizzy (noise -> curl, `amount` exposed) -> out.
json groupedGraph(double amount) {
    json g = json::parse(kBase);
    g["nodes"].push_back({{"name", "noise"}, {"type", "Noise"}, {"params", {{"amplitude", amount}}}});
    g["nodes"].push_back({{"name", "curl"}, {"type", "Curl"}});
    g["connections"].push_back({{"from", "scatter"}, {"to", "noise"}, {"input", "strands"}});
    g["connections"].push_back({{"from", "noise"}, {"to", "curl"}, {"input", "strands"}});
    g["connections"].push_back({{"from", "curl"}, {"to", "out"}, {"input", "strands"}});
    GraphEdit edit(g.dump());
    edit.completeParams();
    edit.collapse({"noise", "curl"}, "frizzy");
    json out = json::parse(edit.toJson());
    for (json& n : out["nodes"]) {
        if (n["name"] == "frizzy") n["params"]["definition"]["expose"] = {{"amount", "noise.amplitude"}};
    }
    return out;
}

StrandsPtr eval(const std::string& graph) {
    GroomData d;
    d.scalp = test::makeSphere(12, 24, 1.0f);
    d.graphJson = graph;
    return Groom(d).evaluate(1.0f, 0.0);
}

// The graph with node `name` of `type` between the scatter and the output.
std::string withNode(const std::string& type, const std::string& paramsJson = {}, std::optional<double> size = std::nullopt) {
    GraphEdit edit(kBase);
    const std::string name = edit.addNode(type, {}, paramsJson, std::nullopt, std::nullopt, size);
    edit.connect("scatter", name, "strands");
    edit.connect(name, "out", "strands");
    return edit.toJson();
}

std::string frizzyFile(int version, const std::string& id = "test.frizzy", double amount = 0.05) {
    json header = {{"id", id}, {"version", version}, {"label", "Frizzy"}, {"category", "Look"}, {"description", "Frizz."},
                   {"paramInfo", {{"amount", {{"label", "Frizz amount"}, {"softMax", 0.5}}}}}};
    return nodeFileFromCompound(groupedGraph(amount).dump(), "frizzy", header.dump());
}

TEST(NodeLibrary, ACompoundSavedAsANodeEvaluatesAsTheCompound) {
    Library lib("roundtrip");
    const std::string text = frizzyFile(1);
    const json file = json::parse(text);
    EXPECT_EQ(file["glnode"], 1);
    EXPECT_EQ(file["id"], "test.frizzy");
    ASSERT_EQ(file["params"].size(), 1u);
    EXPECT_EQ(file["params"][0]["name"], "amount");
    EXPECT_EQ(file["params"][0]["target"], "noise.amplitude");
    EXPECT_EQ(file["params"][0]["label"], "Frizz amount");
    EXPECT_EQ(file["params"][0]["default"], 0.05);
    EXPECT_EQ(file["params"][0]["unit"], "length");
    EXPECT_EQ(file["output"]["type"], "Curves");  // Noise and Curl give what they are fed
    // The file checks, round trips through the folder, and lists.
    checkNodeFile(text);
    const std::string path = saveNodeFile(text);
    EXPECT_TRUE(fs::exists(path));
    EXPECT_EQ(fs::path(path).filename(), "test.frizzy.glnode");
    const auto nodes = libraryNodes();
    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].id, "test.frizzy");
    EXPECT_EQ(nodes[0].version, 1);
    EXPECT_TRUE(nodes[0].error.empty()) << nodes[0].error;

    // Added from the library it evaluates as the compound did, bit for bit; its params show the header.
    const std::string graph = withNode("node:test.frizzy");
    EXPECT_EQ(eval(graph)->positions(), eval(groupedGraph(0.05).dump())->positions());
    GraphEdit edit(graph);
    EXPECT_TRUE(edit.hasNode("frizzy"));
    EXPECT_EQ(edit.nodeType("frizzy"), "Compound");
    const json spec = json::parse(edit.nodeSpecJson("frizzy"));
    EXPECT_EQ(spec["label"], "Frizzy");
    EXPECT_EQ(spec["library"]["id"], "test.frizzy");
    EXPECT_FALSE(spec["library"]["updateAvailable"].get<bool>());
    bool shown = false;
    for (const json& p : spec["params"]) {
        if (p["name"] != "amount") continue;
        shown = true;
        EXPECT_EQ(p["label"], "Frizz amount");
        EXPECT_EQ(p["softMax"], 0.5);
    }
    EXPECT_TRUE(shown);
    // Its value set on the node reaches the inner node, as a compound's exposed param does.
    json tweaked = json::parse(graph);
    for (json& n : tweaked["nodes"]) {
        if (n["name"] == "frizzy") n["params"]["amount"] = 0.2;
    }
    EXPECT_EQ(eval(tweaked.dump())->positions(), eval(groupedGraph(0.2).dump())->positions());

    // The compound it was saved from takes the saved copy (it becomes that library node), same look.
    GraphEdit grouped(groupedGraph(0.05).dump());
    EXPECT_THROW(grouped.updateLibraryNode("frizzy"), GraphError);  // not a library node yet
    EXPECT_TRUE(grouped.updateLibraryNode("frizzy", std::nullopt, "test.frizzy"));
    EXPECT_EQ(json::parse(grouped.nodeSpecJson("frizzy"))["library"]["id"], "test.frizzy");
    EXPECT_EQ(eval(grouped.toJson())->positions(), eval(groupedGraph(0.05).dump())->positions());
}

TEST(NodeLibrary, AGroomOpensAndRendersWithoutTheLibrary) {
    std::string graph;
    {
        Library lib("embedded");
        saveNodeFile(frizzyFile(1));
        graph = saveGraphJson(loadGraphJson(withNode("node:test.frizzy")));
    }
    // The library is gone: the groom holds its copy.
    Library empty("embedded_none");
    EXPECT_FALSE(findLibraryNode("test.frizzy"));
    EXPECT_EQ(eval(graph)->positions(), eval(groupedGraph(0.05).dump())->positions());
    const json spec = json::parse(GraphEdit(graph).nodeSpecJson("frizzy"));
    EXPECT_EQ(spec["library"]["latest"], 0);
    EXPECT_FALSE(spec["library"]["updateAvailable"].get<bool>());
    EXPECT_THROW(GraphEdit(graph).updateLibraryNode("frizzy"), GraphError);
}

TEST(NodeLibrary, AnUnknownNodeHandsItsStrandsOnSayingWhy) {
    Library lib("unknown");
    // By type, with no copy anywhere; and a copy holding a node type this build lacks.
    json byType = json::parse(withNode("Length"));
    for (json& n : byType["nodes"]) {
        if (n["type"] == "Length") n = {{"name", "length"}, {"type", "node:someone.missing"}};
    }
    json broken = json::parse(withNode("Length"));
    for (json& n : broken["nodes"]) {
        if (n["type"] != "Length") continue;
        json definition = json::parse(checkNodeFile(frizzyFile(1)));
        definition["graph"]["nodes"][1]["type"] = "FromTheFuture";
        n = {{"name", "length"}, {"type", "Compound"}, {"params", {{"definition", definition}}}};
    }
    const StrandsPtr plain = eval(plainGraph());
    for (const json& g : {byType, broken}) {
        GraphDocument doc = loadGraphJson(g.dump());
        auto scalp = test::makeSphere(12, 24, 1.0f);
        ExternalInputs ext;
        ext.set("scalp", scalp, scalp->contentHash());
        EvalContext ctx;
        ctx.externals = &ext;
        const StrandsPtr out = doc.graph.evaluateStrands(doc.output, ctx);
        ASSERT_TRUE(out);
        EXPECT_EQ(out->positions(), eval(plainGraph())->positions());
        const auto notes = doc.graph.lastNotes();
        const auto note = notes.find(doc.node("length"));
        ASSERT_NE(note, notes.end());
        EXPECT_NE(note->second.find("Error: library node"), std::string::npos) << note->second;
        // The editors see its ports and that it is a library node.
        const json spec = json::parse(GraphEdit(g.dump()).nodeSpecJson("length"));
        EXPECT_EQ(spec["inputs"][0]["name"], "strands");
        EXPECT_EQ(spec["output"]["type"], "Strands");
        EXPECT_TRUE(spec.contains("library"));
    }
    (void)plain;
    const json spec = json::parse(GraphEdit(byType.dump()).nodeSpecJson("length"));
    EXPECT_FALSE(spec["library"]["embedded"].get<bool>());
}

TEST(NodeLibrary, ANewerLibraryCopyIsAnUpdateTakenByHand) {
    Library lib("update");
    saveNodeFile(frizzyFile(1));
    json graph = json::parse(withNode("node:test.frizzy"));
    for (json& n : graph["nodes"]) {
        if (n["name"] == "frizzy") n["params"]["amount"] = 0.3;
    }
    saveNodeFile(frizzyFile(2, "test.frizzy", 0.1));
    GraphEdit edit(graph.dump());
    json spec = json::parse(edit.nodeSpecJson("frizzy"));
    EXPECT_EQ(spec["library"]["version"], 1);
    EXPECT_EQ(spec["library"]["latest"], 2);
    EXPECT_TRUE(spec["library"]["updateAvailable"].get<bool>());
    // The groom keeps its copy until updated.
    EXPECT_EQ(eval(graph.dump())->positions(), eval(groupedGraph(0.3).dump())->positions());
    EXPECT_TRUE(edit.updateLibraryNode("frizzy"));
    spec = json::parse(edit.nodeSpecJson("frizzy"));
    EXPECT_EQ(spec["library"]["version"], 2);
    EXPECT_FALSE(spec["library"]["updateAvailable"].get<bool>());
    EXPECT_FALSE(edit.updateLibraryNode("frizzy"));
    // The value the user set stays; links stay.
    EXPECT_EQ(eval(edit.toJson())->positions(), eval(groupedGraph(0.3).dump())->positions());
    EXPECT_THROW(edit.updateLibraryNode("scatter"), GraphError);
}

TEST(NodeLibrary, EarlierFoldersHideLaterOnesAndTheMenuListsByCategory) {
    Library lib("precedence");
    lib.write("a", "frizzy.glnode", frizzyFile(3));
    lib.write("b", "frizzy.glnode", frizzyFile(7));
    json other = json::parse(frizzyFile(1, "test.mine"));
    other["category"] = "";
    other["label"] = "Mine";
    lib.write("b", "mine.glnode", other.dump());
    json custom = json::parse(frizzyFile(1, "test.custom"));
    custom["category"] = "Studio Looks";
    custom["label"] = "Custom";
    lib.write("b", "custom.glnode", custom.dump());
    lib.write("b", "broken.glnode", R"({"glnode": 1, "id": "test.broken", "version": 1})");
    setEnv("GROOMLAB_NODE_PATH", lib.dir("a") + (fs::path::preferred_separator == '\\' ? ";" : ":") + lib.dir("b"));
    const auto node = findLibraryNode("test.frizzy");
    ASSERT_TRUE(node);
    EXPECT_EQ(node->version, 3);  // a's
    EXPECT_FALSE(findLibraryNode("test.broken"));

    const json menu = json::parse(libraryNodesJson());
    std::map<std::string, json> byId;
    for (const json& n : menu["nodes"]) byId[n["id"]] = n;
    ASSERT_EQ(byId.size(), 3u);
    EXPECT_EQ(byId["test.frizzy"]["type"], "node:test.frizzy");
    EXPECT_EQ(byId["test.frizzy"]["category"], "look");
    EXPECT_EQ(byId["test.frizzy"]["categoryTitle"], "Look");
    EXPECT_EQ(byId["test.mine"]["categoryTitle"], "My Nodes");
    EXPECT_EQ(byId["test.custom"]["categoryTitle"], "Studio Looks");
    EXPECT_LT(byId["test.frizzy"]["categoryOrder"].get<int>(), byId["test.custom"]["categoryOrder"].get<int>());
    EXPECT_LT(byId["test.custom"]["categoryOrder"].get<int>(), byId["test.mine"]["categoryOrder"].get<int>());
    EXPECT_EQ(byId["test.frizzy"]["inputs"][0]["name"], "strands");
    ASSERT_EQ(menu["errors"].size(), 1u);
    EXPECT_EQ(menu["errors"][0]["id"], "test.broken");
    EXPECT_NE(menu["errors"][0]["error"].get<std::string>().find("label"), std::string::npos);
    EXPECT_EQ(menu["dirs"].size(), 3u);  // a, b, the user's
}

TEST(NodeLibrary, BadFilesSayWhatIsWrong) {
    const json good = json::parse(frizzyFile(1));
    auto expectError = [&](const std::function<void(json&)>& spoil, const std::string& says) {
        json file = good;
        spoil(file);
        try {
            checkNodeFile(file.dump());
            ADD_FAILURE() << "no error; wanted " << says;
        } catch (const GraphError& e) {
            EXPECT_NE(std::string(e.what()).find(says), std::string::npos) << e.what() << " (wanted " << says << ")";
        }
    };
    expectError([](json& f) { f.erase("glnode"); }, "no \"glnode\"");
    expectError([](json& f) { f["glnode"] = 99; }, "update GroomLab");
    expectError([](json& f) { f["id"] = "nodots"; }, "two parts");
    expectError([](json& f) { f["id"] = "Bad.Id"; }, "a-z");
    expectError([](json& f) { f["version"] = 0; }, "version");
    expectError([](json& f) { f["label"] = " "; }, "label");
    expectError([](json& f) { f["author"] = 3; }, "\"author\" must be text");
    expectError([](json& f) { f.erase("graph"); }, "\"graph\"");
    expectError([](json& f) { f["graph"]["nodes"][1]["type"] = "NoSuchNode"; }, "graph: ");
    expectError([](json& f) { f["inputs"] = json::array(); }, "does not list");
    expectError([](json& f) { f["inputs"][0]["type"] = "Hair"; }, "inputs: ");
    expectError([](json& f) { f["params"][0]["target"] = "nowhere.amplitude"; }, "params[0]: the graph has no node");
    expectError([](json& f) { f["params"][0]["target"] = "noise.nothing"; }, "has no param 'nothing'");
    expectError([](json& f) { f["params"][0]["name"] = "repeat"; }, "taken");
    expectError([](json& f) { f["params"][0]["min"] = "low"; }, "\"min\" must be a number");
    expectError([](json& f) { f["params"][0]["default"] = "loud"; }, "does not fit noise.amplitude");
    expectError([](json& f) { f["output"]["type"] = "Mesh"; }, "\"output\" says Mesh");
    EXPECT_THROW(checkNodeFile("not json"), GraphError);
    EXPECT_EQ(nodeIdError("studio.hair.soft_frizz"), "");
    EXPECT_EQ(suggestNodeId("Soft Frizz", "Nick Rutlinh"), "nick_rutlinh.soft_frizz");
    EXPECT_EQ(suggestNodeId("Soft Frizz", ""), "my.soft_frizz");
    EXPECT_EQ(libraryNodeName("Wind-swept Ends"), "windSweptEnds");
}

TEST(NodeLibrary, ANodeFitsTheScalpItIsAddedTo) {
    Library lib("fit");
    json file = json::parse(frizzyFile(1));
    file["size"] = 20.0;
    saveNodeFile(file.dump());
    GraphEdit edit(withNode("node:test.frizzy", {}, 2.0));
    const json definition = json::parse(edit.toJson())["nodes"][3]["params"]["definition"];
    EXPECT_EQ(definition["size"], 2.0);
    EXPECT_NEAR(definition["params"][0]["default"].get<double>(), 0.005, 1e-9);
    EXPECT_NEAR(definition["params"][0]["softMax"].get<double>(), 0.05, 1e-9);
    for (const json& n : definition["graph"]["nodes"]) {
        if (n["name"] == "noise") EXPECT_NEAR(n["params"]["amplitude"].get<double>(), 0.005, 1e-9);
    }
}

TEST(NodeLibrary, TheShippedNodesCheckAndChangeTheStrands) {
    Library lib("shipped");
    setShippedNodeDir((fs::path(GROOMLAB_SHIPPED_PRESETS).parent_path() / "nodes").generic_string());
    const auto nodes = libraryNodes();
    ASSERT_GE(nodes.size(), 3u);
    const StrandsPtr plain = eval(plainGraph());
    for (const LibraryNode& node : nodes) {
        SCOPED_TRACE(node.id);
        EXPECT_TRUE(node.error.empty()) << node.error;
        EXPECT_EQ(node.id.rfind("groomlab.", 0), 0u);
        const std::string graph = withNode(libraryNodeType(node.id), {}, 2.0);
        GraphDocument doc = loadGraphJson(graph);
        auto scalp = test::makeSphere(12, 24, 1.0f);
        ExternalInputs ext;
        ext.set("scalp", scalp, scalp->contentHash());
        EvalContext ctx;
        ctx.externals = &ext;
        const StrandsPtr out = doc.graph.evaluateStrands(doc.output, ctx);
        ASSERT_TRUE(out);
        EXPECT_TRUE(doc.graph.lastNotes().empty());
        EXPECT_EQ(out->strandCount(), plain->strandCount());
        EXPECT_TRUE(out->positions() != plain->positions() || out->strandChannelNames() != plain->strandChannelNames() ||
                    out->widths() != plain->widths());
    }
    setShippedNodeDir("");
}

}  // namespace
}  // namespace groomlab

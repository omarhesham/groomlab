// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <vector>

namespace groomlab {

// The add-node menu every host shows (Maya's Graph Editor, the app's node graph, Blender's node
// editor): its categories in the order a groom is built, each node type's category, its display
// label and the words a search finds it by. operatorSpecsJson writes these into each spec
// ("category", "categoryTitle", "categoryOrder", "order", "label", "keywords", "hidden"), so no
// host keeps its own list.
//
// The category here is the menu's. An operator's own category() is its role in a chain (where a
// preset's look starts: a source or grow node), written as the spec's "role".
struct NodeCategory {
    std::string id;     // "shape"
    std::string title;  // "Shape"
};

// The categories in menu order.
const std::vector<NodeCategory>& nodeCategories();

struct NodeCatalogEntry {
    std::string type;      // the operator's type ("Reference"): what files store, never renamed
    std::string category;  // a NodeCategory id
    std::string label;     // what every host shows ("Linked Groom"); empty: the type's words spaced
    std::string keywords;  // words a search finds it by that its name does not say ("symmetry")
    bool hidden = false;   // made by grouping or by a compound, not from the add-node menu
};

// Every catalogued type, in menu order within its category.
const std::vector<NodeCatalogEntry>& nodeCatalog();

// The entry for a type, or null for one the catalog does not list.
const NodeCatalogEntry* nodeCatalogEntry(const std::string& type);

// A type's display label: the entry's, else its CamelCase spaced ("BlendNeighbors" -> "Blend
// Neighbors").
std::string nodeLabel(const std::string& type);

}  // namespace groomlab

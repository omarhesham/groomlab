# SPDX-License-Identifier: Apache-2.0
"""engine/ui/*.json, the UI structure every host reads (docs/UI_DEFINITIONS.md): each file valid
against engine/ui/ui.schema.json, every id a container lists defined, every reference found, every
action used somewhere. With the groomlab_core module importable, also against the core: shortcut ids
(shortcuts.h), the marking menu's tools (brush_catalog.h) and the built-in tube shapes.

    python tests/engine/ui/check_ui_definitions.py [engine/ui folder]

Exit code 0 when all of it passed (ctest ui.definitions). Needs no package: the schema subset the
files use is checked here.
"""

import io
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
FILES = ("actions", "menus", "palette", "lists", "choices", "marking_menu", "graph_ui")

errors = []


def fail(message):
    if message not in errors:  # a list referenced from two places: said once
        errors.append(message)


# -- The schema subset (draft 2020-12 keywords the schema uses) -------------------------------------

_TYPES = {"string": str, "boolean": bool, "array": list, "object": dict, "null": type(None)}


def _is_type(value, name):
    if name == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if name == "number":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    return isinstance(value, _TYPES[name])


def validate(value, schema, root, where):
    """Problems of `value` against `schema` (a list of "where: what")."""
    problems = []
    if "$ref" in schema:
        node = root
        for part in schema["$ref"].lstrip("#/").split("/"):
            node = node[part]
        return validate(value, node, root, where)
    if "type" in schema:
        types = schema["type"] if isinstance(schema["type"], list) else [schema["type"]]
        if not any(_is_type(value, t) for t in types):
            return ["%s: %r is not %s" % (where, value, "/".join(types))]
    if "const" in schema and value != schema["const"]:
        problems.append("%s: %r is not %r" % (where, value, schema["const"]))
    if "enum" in schema and value not in schema["enum"]:
        problems.append("%s: %r is not one of %r" % (where, value, schema["enum"]))
    if isinstance(value, str):
        if "pattern" in schema and not re.search(schema["pattern"], value):
            problems.append("%s: %r does not match %s" % (where, value, schema["pattern"]))
        if len(value) < schema.get("minLength", 0):
            problems.append("%s: empty" % where)
    if _is_type(value, "number"):
        if "minimum" in schema and value < schema["minimum"]:
            problems.append("%s: %r < %r" % (where, value, schema["minimum"]))
        if "maximum" in schema and value > schema["maximum"]:
            problems.append("%s: %r > %r" % (where, value, schema["maximum"]))
    if isinstance(value, list):
        if len(value) < schema.get("minItems", 0) or len(value) > schema.get("maxItems", len(value)):
            problems.append("%s: %d items" % (where, len(value)))
        if "items" in schema:
            for i, item in enumerate(value):
                problems += validate(item, schema["items"], root, "%s[%d]" % (where, i))
    if isinstance(value, dict):
        for key in schema.get("required", []):
            if key not in value:
                problems.append("%s: no %r" % (where, key))
        properties = schema.get("properties", {})
        for key, item in value.items():
            if "propertyNames" in schema:
                problems += validate(key, schema["propertyNames"], root, "%s (key %r)" % (where, key))
            if key in properties:
                problems += validate(item, properties[key], root, "%s/%s" % (where, key))
            elif schema.get("additionalProperties") is False:
                problems.append("%s: unknown key %r" % (where, key))
            elif isinstance(schema.get("additionalProperties"), dict):
                problems += validate(item, schema["additionalProperties"], root, "%s/%s" % (where, key))
    for keyword in ("anyOf", "oneOf"):
        if keyword in schema:
            results = [validate(value, s, root, where) for s in schema[keyword]]
            passing = [r for r in results if not r]
            if not passing:
                # The closest choice's problems (an entry of the wrong shape: what is wrong with it),
                # past the choices of another type altogether.
                shaped = [r for r in results if not (r[0].startswith(where + ":") and " is not " in r[0])]
                problems += min(shaped or results, key=len)
            elif keyword == "oneOf" and len(passing) > 1:
                problems.append("%s: matches %d of the oneOf choices" % (where, len(passing)))
    return problems


# -- The files ------------------------------------------------------------------------------------

def load(folder):
    with io.open(os.path.join(folder, "ui.schema.json"), encoding="utf-8") as f:
        schema = json.load(f)
    merged = {}
    for name in FILES:
        path = os.path.join(folder, name + ".json")
        with io.open(path, encoding="utf-8") as f:
            data = json.load(f)
        for problem in validate(data, schema, schema, name + ".json"):
            fail(problem)
        for key, value in data.items():
            if key in ("$schema", "version"):
                continue
            if key in merged:
                fail("%s.json: %r is in another file too" % (name, key))
            merged[key] = value
    extra = sorted(f for f in os.listdir(folder) if f.endswith(".json") and f[:-5] not in FILES + ("ui.schema",))
    if extra:
        fail("files no host reads (add them to FILES in every host's loader): %r" % extra)
    return merged


def get(doc, path):
    node = doc
    for part in path.split("/"):
        if not isinstance(node, dict) or part not in node:
            return None
        node = node[part]
    return node


def walk(doc, entries, where, used):
    """Every entry of a container: ids defined, references found; used ids collected."""
    actions = doc["actions"]
    for i, entry in enumerate(entries):
        here = "%s[%d]" % (where, i)
        if entry == "-":
            continue
        if isinstance(entry, str):
            entry = {"id": entry}
        if "include" in entry or "ref" in entry:
            path = entry.get("include") or entry.get("ref")
            target = get(doc, path)
            if target is None:
                fail("%s: no %s" % (here, path))
            elif "items" not in entry:
                walk(doc, target["items"] if isinstance(target, dict) else target, path, used)
        if "items" in entry:
            walk(doc, entry["items"], here, used)
        if "generator" in entry:
            if entry.get("action"):
                used.add(entry["action"])
                if entry["action"] not in actions:
                    fail("%s: generator action %r is not in actions.json" % (here, entry["action"]))
            if entry["generator"] == "choices":
                options = doc["choices"].get(entry.get("choices", ""))
                if options is None:
                    fail("%s: no choices %r" % (here, entry.get("choices")))
                elif entry.get("labelField") and any(entry["labelField"] not in c for c in options["items"]):
                    fail("%s: an option of %s has no %r" % (here, entry["choices"], entry["labelField"]))
        elif "id" in entry and "submenu" not in entry and "ref" not in entry:
            used.add(entry["id"])
            if entry["id"] not in actions:
                fail("%s: %r is not in actions.json" % (here, entry["id"]))


def containers(doc):
    """(where, entries) of every container in the files."""
    out = []
    for name, value in doc["menus"].items():
        out.append(("menus/" + name, value["items"] if isinstance(value, dict) else value))
    palette = doc["palette"]
    out.append(("palette/output", palette["output"]))
    out.append(("palette/empty", palette.get("empty", {}).get("items", [])))
    for name, header in palette.get("header", {}).items():
        out.append(("palette/header/" + name, header.get("items", [])))
    for name, lists in doc["lists"].items():
        for key, value in lists.items():
            if isinstance(value, (list, dict)):
                out.append(("lists/%s/%s" % (name, key), value["items"] if isinstance(value, dict) else value))
    for key, value in doc["graph"].items():
        if key.endswith("Menu") or key in ("presetItem",):
            out.append(("graph/" + key, value["items"] if isinstance(value, dict) else value))
    out.append(("markingMenu/tail", [e for e in doc["markingMenu"]["tail"] if isinstance(e, str)]))
    return out


def check(doc):
    used = set()
    for where, entries in containers(doc):
        walk(doc, entries, where, used)
    for aid, action in sorted(doc["actions"].items()):
        if aid not in used:
            fail("actions.json: %r is listed nowhere" % aid)
        if "{" in action.get("label", "") and not re.search(r"\{\w+\}", action["label"]):
            fail("actions.json: %r has a stray brace in its label" % aid)
    for name, options in doc["choices"].items():
        values = [c["value"] for c in options["items"]]
        if len(set(values)) != len(values):
            fail("choices.json: %s has a value twice" % name)
        if "default" in options and options["default"] not in values:
            fail("choices.json: %s's default %r is not one of its values" % (name, options["default"]))
    ids = [t["id"] for t in doc["palette"]["tabs"]]
    aliases = [a for t in doc["palette"]["tabs"] for a in t.get("aliases", [])]
    if len(set(ids + aliases)) != len(ids + aliases):
        fail("palette.json: a tab id or alias twice")
    radial = [r["position"] for r in doc["markingMenu"]["radial"]]
    if len(set(radial)) != len(radial):
        fail("marking_menu.json: a radial position twice")


def check_core(doc):
    """Against the core, when its Python module is importable (the build's, on PYTHONPATH)."""
    try:
        import groomlab_core
    except ImportError:
        print("groomlab_core not importable: the core cross-checks are skipped")
        return
    rows = set()
    for host in ("maya", "blender", "app"):
        rows.update(r["id"] for r in groomlab_core.shortcuts(host))
    for aid, action in doc["actions"].items():
        if action.get("shortcut") and action["shortcut"] not in rows:
            fail("actions.json: %r's shortcut %r is not in the core's shortcuts.h" % (aid, action["shortcut"]))
        if "shortcutPart" in action:
            keys = [r["keys"] for h in ("maya", "blender", "app") for r in groomlab_core.shortcuts(h)
                    if r["id"] == action["shortcut"]]
            if keys and action["shortcutPart"] >= len(keys[0].split(" / ")):
                fail("actions.json: %r's shortcutPart is past %r" % (aid, keys[0]))
    tools = set(t["id"] for t in groomlab_core.brush_catalog())
    menu = doc["markingMenu"]
    for entry in menu["radial"] + [e for e in menu["tail"] if isinstance(e, dict)]:
        if entry["tool"] not in tools:
            fail("marking_menu.json: no tool %r in the brush catalog" % entry["tool"])
    for name in menu["overflow"].get("exclude", []):
        if name not in tools:
            fail("marking_menu.json: overflow excludes %r, not in the brush catalog" % name)
    if hasattr(groomlab_core, "tube_shapes"):
        builtin = [s if isinstance(s, str) else s.get("name") for s in groomlab_core.tube_shapes()]
        shapes = [c["value"] for c in doc["choices"]["tubeShape"]["items"]]
        if builtin and shapes != list(builtin):
            fail("choices.json: tubeShape %r is not the core's built-in shapes %r" % (shapes, builtin))


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "engine", "ui")
    doc = load(folder)
    if not errors:
        check(doc)
        check_core(doc)
    for message in errors:
        print("FAIL " + message)
    print("%d problem%s in %s" % (len(errors), "" if len(errors) == 1 else "s", folder))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())

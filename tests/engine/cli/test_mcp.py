# SPDX-License-Identifier: Apache-2.0
"""groomlab-cli's MCP server played as an AI client plays it: a scripted session over stdin/stdout.

    python test_mcp.py <groomlab-cli> <work folder>

Starts `groomlab-cli mcp --root <work folder>`, then: initialize, tools/list (every tool has a
schema), the discovery tools, new_groom on the Nika sample, apply_preset, add_node, set_params,
sample_scalp, add_guides, set_tube, add_script_node with an error and then fixed, evaluate (no
errors), render_preview (a real PNG with something drawn, the same twice), save_groom and open_groom
(the same strands), export USD, save_node, a path outside the root refused, undo. Standard library
only.
"""

import base64
import json
import os
import shutil
import struct
import subprocess
import sys
import zlib


class Client:
    def __init__(self, exe, root):
        self.log = open(os.path.join(root, "server_stderr.txt"), "wb")
        self.proc = subprocess.Popen([exe, "mcp", "--root", root], cwd=root, stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=self.log)
        self.next_id = 1

    def send(self, message):
        self.proc.stdin.write((json.dumps(message) + "\n").encode("utf-8"))
        self.proc.stdin.flush()

    def request(self, method, params=None):
        rid = self.next_id
        self.next_id += 1
        message = {"jsonrpc": "2.0", "id": rid, "method": method}
        if params is not None:
            message["params"] = params
        self.send(message)
        line = self.proc.stdout.readline()
        if not line:
            raise AssertionError("the server closed stdout (see server_stderr.txt)")
        response = json.loads(line)
        assert response.get("jsonrpc") == "2.0" and response.get("id") == rid, response
        return response

    def call(self, name, arguments=None, expect_error=False):
        response = self.request("tools/call", {"name": name, "arguments": arguments or {}})
        assert "result" in response, response
        result = response["result"]
        if result.get("isError") != expect_error:
            raise AssertionError("%s %s: %s" % (name, "succeeded" if expect_error else "failed", json.dumps(result)[:2000]))
        texts = [c["text"] for c in result["content"] if c["type"] == "text"]
        data = json.loads(texts[-1]) if texts else None
        return data, result

    def close(self):
        self.proc.stdin.close()
        code = self.proc.wait(timeout=60)
        self.log.close()
        return code


def png_pixels(data):
    """Width, height and the RGB rows of an 8-bit RGB PNG (any filter)."""
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos, idat, width, height = 8, b"", 0, 0
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            width, height, depth, color = struct.unpack(">IIBB", body[:10])
            assert depth == 8 and color == 2, "an 8-bit RGB PNG"
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    raw = zlib.decompress(idat)
    stride = width * 3
    rows, prev = [], bytearray(stride)
    for y in range(height):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - 3] if i >= 3 else 0
            b = prev[i]
            c = prev[i - 3] if i >= 3 else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(line))
        prev = line
    return width, height, rows


def main():
    exe, root = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    shutil.rmtree(root, ignore_errors=True)
    os.makedirs(root)
    c = Client(exe, root)
    try:
        # ---- the handshake ----
        init = c.request("initialize", {"protocolVersion": "2025-06-18", "capabilities": {},
                                        "clientInfo": {"name": "test", "version": "1"}})["result"]
        assert init["protocolVersion"] == "2025-06-18", init
        assert "tools" in init["capabilities"] and init["serverInfo"]["name"] == "groomlab", init
        c.send({"jsonrpc": "2.0", "method": "notifications/initialized"})
        assert c.request("ping")["result"] == {}
        tools = c.request("tools/list")["result"]["tools"]
        names = {t["name"] for t in tools}
        for t in tools:
            assert t["description"] and t["inputSchema"]["type"] == "object" and isinstance(t["inputSchema"]["properties"], dict), t
        for wanted in ["list_nodes", "describe_node", "list_presets", "list_sample_meshes", "get_docs", "new_groom", "open_groom",
                       "apply_preset", "get_graph", "add_node", "remove_node", "connect", "disconnect", "set_params", "add_guides",
                       "set_tube", "add_script_node", "save_node", "undo", "evaluate", "render_preview", "save_groom", "export"]:
            assert wanted in names, wanted
        assert c.request("tools/call", {"name": "no_such_tool", "arguments": {}})["error"]["code"] == -32602
        assert c.request("no/such/method")["error"]["code"] == -32601

        # ---- discover ----
        nodes, _ = c.call("list_nodes")
        assert nodes["count"] > 40, nodes["count"]
        assert any(n["type"] == "node:groomlab.wave" for g in nodes["categories"] for n in g["nodes"]), "library nodes listed"
        curl, _ = c.call("describe_node", {"type": "Curl"})
        assert curl["params"], curl
        presets, _ = c.call("list_presets")
        assert any(p["name"] == "hair_basics" for p in presets["presets"])
        meshes, _ = c.call("list_sample_meshes")
        assert any(m["name"] == "nika" for m in meshes["meshes"]), meshes
        # Buddy: its body (buddy.obj), not the hosts' rigs (buddy_rig.ma, buddy_rig.fbx).
        buddy = [m for m in meshes["meshes"] if m["name"] == "buddy"]
        assert len(buddy) == 1 and buddy[0]["file"] == "buddy.obj" and "rigged" in buddy[0]["description"], meshes
        docs, _ = c.call("get_docs", {"topic": "tube guides"})
        assert docs["text"], docs
        c.call("list_nodes", {"bogus": 1}, expect_error=True)  # an unknown argument says so

        # ---- build ----
        made, _ = c.call("new_groom", {"name": "nika", "mesh": "nika"})
        assert made["summary"]["strands"] > 0 and made["summary"]["errors"] == [], made
        applied, _ = c.call("apply_preset", {"preset": "hair_basics"})
        assert applied["summary"]["strands"] > 0, applied
        graph, _ = c.call("get_graph")
        color = next(n["name"] for n in graph["nodes"] if n["type"] == "HairColor")
        added, _ = c.call("add_node", {"type": "Curl", "after": color})
        curl_node = added["node"]
        param = next(p for p in curl["params"] if p["kind"] == "float" and not p.get("advanced") and "default" in p)
        value = param["default"] * 0.5 if param["default"] else 0.1
        if "max" in param:
            value = min(value, param["max"])
        set_, _ = c.call("set_params", {"node": curl_node, "params": {param["name"]: value}})
        assert set_["set"] == [param["name"]], set_
        c.call("set_params", {"node": curl_node, "params": {"noSuchParam": 1}}, expect_error=True)

        roots, _ = c.call("sample_scalp", {"count": 6, "min_up": 0.5})
        assert len(roots["points"]) == 6, roots
        guides = [{"root": p["p"], "length": 12, "droop": 0.6} for p in roots["points"][:2]]
        top = roots["points"][2]["p"]
        guides.append({"points": [top, [top[0], top[1] + 4, top[2] + 2], [top[0], top[1] + 6, top[2] + 8]]})
        added_guides, _ = c.call("add_guides", {"guides": guides})
        assert len(added_guides["ids"]) == 3 and added_guides["summary"]["guides"] >= 3, added_guides
        tubes, _ = c.call("set_tube", {"guides": added_guides["ids"], "shape": "ribbon", "width": [0.6, 0.2],
                                       "aspect": 0.3, "twist_degrees": 30})
        assert tubes["guides"] == 3, tubes
        listed, _ = c.call("get_guides")
        assert all(g["tube"]["shape"] == "ribbon" for g in listed["guides"]), listed

        broken, _ = c.call("add_script_node", {"label": "Lift", "script": "per point {\n    P.y += $amount * ;\n}",
                                               "params": [{"name": "amount", "type": "length", "default": 0.5}]},
                           expect_error=True)
        assert broken["errors"] and broken["errors"][0]["line"] == 2, broken
        fixed, _ = c.call("add_script_node", {"label": "Lift", "script": "per point {\n    P.y += $amount * t;\n}",
                                              "params": [{"name": "amount", "type": "length", "default": 0.5}],
                                              "after": curl_node})
        script_node = fixed["node"]
        assert fixed["summary"]["errors"] == [], fixed

        # ---- check ----
        evaluated, _ = c.call("evaluate")
        assert evaluated["errors"] == [] and evaluated["strands"] > 0 and "nodeMs" in evaluated, evaluated
        strands = evaluated["strands"]

        # ---- see ----
        info, result = c.call("render_preview", {"views": ["front", "three_quarter"], "size": 256, "save_to": "out/preview.png"})
        image = next(x for x in result["content"] if x["type"] == "image")
        assert image["mimeType"] == "image/png"
        png = base64.b64decode(image["data"])
        width, height, rows = png_pixels(png)
        assert (width, height) == (256, 128), (width, height)
        colors = {row[i:i + 3] for row in rows for i in range(0, len(row), 3)}
        assert len(colors) > 50, "the preview draws something (%d colors)" % len(colors)
        assert os.path.getsize(os.path.join(root, "out", "preview.png")) == len(png)
        _, again = c.call("render_preview", {"views": ["front", "three_quarter"], "size": 256})
        assert next(x for x in again["content"] if x["type"] == "image")["data"] == image["data"], "the same preview twice"

        # ---- deliver ----
        node_file, _ = c.call("save_node", {"node": script_node, "path": "out/lift.glnode"})
        assert os.path.isfile(os.path.join(root, "out", "lift.glnode")), node_file
        saved, _ = c.call("save_groom", {"path": "out/nika.groom"})
        assert os.path.isfile(os.path.join(root, "out", "nika.groom")), saved
        opened, _ = c.call("open_groom", {"path": "out/nika.groom", "name": "nika2"})
        assert opened["summary"]["strands"] == strands and opened["summary"]["errors"] == [], opened
        exported, _ = c.call("export", {"groom": "nika2", "path": "out/nika.usda", "frames": "1"})
        with open(os.path.join(root, "out", "nika.usda"), encoding="utf-8") as f:
            assert "BasisCurves" in f.read(), exported
        assert exported["strandsPerFrame"] == strands, exported

        # ---- safety ----
        outside, _ = c.call("save_groom", {"groom": "nika", "path": "../outside.groom"}, expect_error=True)
        assert "outside" in outside["error"], outside
        assert not os.path.exists(os.path.join(os.path.dirname(root), "outside.groom"))
        c.call("open_groom", {"path": os.path.join(os.path.dirname(root), "x.groom")}, expect_error=True)
        c.call("new_groom", {"name": "bad", "mesh": "../../etc/scalp.obj"}, expect_error=True)
        c.call("evaluate", {}, expect_error=True)  # two grooms open: say which

        # ---- undo ----
        undone, _ = c.call("undo", {"groom": "nika"})
        assert undone["undone"] == ["add script node"], undone
        graph, _ = c.call("get_graph", {"groom": "nika"})
        assert all(n["name"] != script_node for n in graph["nodes"]), "the script node is gone"

        # ---- hand-placed feathers: the preset's guide group made, each guide added to it a feather ----
        bird, _ = c.call("apply_preset", {"groom": "nika2", "preset": "hand_placed_feathers"})
        assert bird["groupsMade"] == ["feathers"] and bird["summary"]["errors"] == [], bird
        assert bird["summary"]["strands"] == 0, bird
        drawn, _ = c.call("add_guides", {"groom": "nika2", "group": "feathers", "guides": guides[:2]})
        assert drawn["summary"]["strands"] == 2 * (1 + 140), drawn
    finally:
        code = c.close()
    assert code == 0, "the server exited with %d" % code
    print("mcp session: ok")


if __name__ == "__main__":
    main()

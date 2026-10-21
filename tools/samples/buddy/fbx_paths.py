# SPDX-License-Identifier: Apache-2.0
"""Rewrite the absolute paths a binary FBX embeds (the exporter writes the file's and the Maya
scene's full paths: someone's home and work folders) to bare file names.

Usage: python fbx_paths.py <file.fbx>     (rewrites it in place, prints what changed)

Plain Python 3.7+ (mayapy runs it too). The binary FBX layout: a 27-byte header, node records
(end offset, property count, property bytes, name, properties, children ended by a null record),
a null record, the footer. String properties carry their length, so the records are written again
with new offsets; every other byte is kept, and the footer's padding to 16 bytes is recomputed.
"""

import re
import struct
import sys

_SIZES = {b"Y": 2, b"C": 1, b"I": 4, b"F": 4, b"D": 8, b"L": 8}
_ARRAYS = b"fdlib"
_ABSOLUTE = re.compile(rb"^(?:[A-Za-z]:[/\\]|[/\\]{2}|/)")


def _read(data, pos, wide):
    head = "<QQQ" if wide else "<III"
    end, count, plen = struct.unpack_from(head, data, pos)
    pos += struct.calcsize(head)
    nlen = data[pos]
    pos += 1
    if end == 0:
        return None, pos
    name = data[pos:pos + nlen]
    pos += nlen
    props = []
    pend = pos + plen
    while pos < pend:
        t = data[pos:pos + 1]
        pos += 1
        if t in (b"S", b"R"):
            (n,) = struct.unpack_from("<I", data, pos)
            props.append((t, data[pos + 4:pos + 4 + n]))
            pos += 4 + n
        elif t in _SIZES:
            props.append((t, data[pos:pos + _SIZES[t]]))
            pos += _SIZES[t]
        elif t in _ARRAYS:
            _, _, n = struct.unpack_from("<III", data, pos)
            props.append((t, data[pos:pos + 12 + n]))
            pos += 12 + n
        else:
            raise ValueError("unknown FBX property type %r at %d" % (t, pos - 1))
    assert len(props) == count and pos == pend
    children = None
    if pos < end:
        children = []
        while True:
            child, pos = _read(data, pos, wide)
            if child is None:
                break
            children.append(child)
    assert pos == end
    return (name, props, children), pos


def _write(out, node, wide):
    name, props, children = node
    head = "<QQQ" if wide else "<III"
    start = len(out)
    out += b"\0" * struct.calcsize(head)
    out.append(len(name))
    out += name
    pstart = len(out)
    for t, v in props:
        out += t
        if t in (b"S", b"R"):
            out += struct.pack("<I", len(v))
        out += v
    plen = len(out) - pstart
    if children is not None:
        for child in children:
            _write(out, child, wide)
        out += b"\0" * (struct.calcsize(head) + 1)
    struct.pack_into(head, out, start, len(out), len(props), plen)


def _bare(value):
    """A path's last part; the rest of a string untouched."""
    if not _ABSOLUTE.match(value):
        return value
    return re.split(rb"[/\\]", value.rstrip(b"/\\"))[-1]


def _rewrite(node, changes):
    name, props, children = node
    # The Maya project folder says nothing about the file: emptied.
    project = name == b"P" and props and props[0][1].endswith(b"|ApplicationActiveProject")
    for i, (t, v) in enumerate(props):
        if t == b"S":
            new = b"" if project and i == len(props) - 1 else _bare(v)
            if new != v:
                changes.append((v, new))
                props[i] = (t, new)
    for child in children or ():
        _rewrite(child, changes)


def parse(data):
    if not data.startswith(b"Kaydara FBX Binary  \x00"):
        raise ValueError("not a binary FBX")
    (version,) = struct.unpack_from("<I", data, 23)
    wide = version >= 7500
    pos, nodes = 27, []
    while True:
        node, pos = _read(data, pos, wide)
        if node is None:
            break
        nodes.append(node)
    return version, nodes, data[pos:]


def serialize(version, nodes, footer):
    wide = version >= 7500
    out = bytearray(b"Kaydara FBX Binary  \x00\x1a\x00" + struct.pack("<I", version))
    for node in nodes:
        _write(out, node, wide)
    out += b"\0" * (25 if wide else 13)
    # Footer: an id (16 bytes), 4 zero bytes, zeros so the file ends on a 16-byte boundary (as the
    # FBX SDK writes it; 16 when it already would), the version, 120 zero bytes, the magic (16).
    tail = footer[-(4 + 120 + 16):]
    out += footer[:20]
    pad = -(len(out) + len(tail)) % 16
    out += b"\0" * (pad or 16)
    out += tail
    return bytes(out)


def rewrite(path):
    data = open(path, "rb").read()
    version, nodes, footer = parse(data)
    if serialize(version, nodes, footer) != data:
        raise ValueError("%s: the FBX doesn't read back byte for byte, not rewriting it" % path)
    changes = []
    for node in nodes:
        _rewrite(node, changes)
    if changes:
        with open(path, "wb") as f:
            f.write(serialize(version, nodes, footer))
    return changes


def scan(path, words=(b"NickRutlinh", b"Users", b"D:", b"worktree", b".claude")):
    """Readable strings in the file that still hold a path or one of `words`."""
    data = open(path, "rb").read()
    _, nodes, _ = parse(data)
    found = []

    def walk(node):
        for t, v in node[1]:
            if t == b"S" and (_ABSOLUTE.match(v) or any(w in v for w in words)):
                found.append(v)
        for child in node[2] or ():
            walk(child)

    for node in nodes:
        walk(node)
    return found


if __name__ == "__main__":
    for p in sys.argv[1:]:
        for old, new in rewrite(p):
            print("%s: %s -> %s" % (p, old.decode("utf-8", "replace"), new.decode("utf-8", "replace")))
        left = scan(p)
        print("%s: %d path strings left" % (p, len(left)), [v.decode("utf-8", "replace") for v in left])

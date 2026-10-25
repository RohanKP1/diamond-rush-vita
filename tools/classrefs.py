"""Dump external API references (class + method/field) from .class files.

Usage: python -I tools/classrefs.py <dir-with-classes>
Used to scope which MIDP / Nokia APIs the C++ shim layer must implement.
"""
import struct
import sys
from collections import defaultdict
from pathlib import Path


def parse_pool(data):
    count = struct.unpack(">H", data[8:10])[0]
    pool = [None] * count
    i, off = 1, 10
    while i < count:
        tag = data[off]
        if tag == 1:
            ln = struct.unpack(">H", data[off + 1:off + 3])[0]
            pool[i] = ("utf8", data[off + 3:off + 3 + ln].decode("utf-8", "replace"))
            off += 3 + ln
        elif tag in (3, 4):
            off += 5
        elif tag in (5, 6):
            off += 9
            i += 1
        elif tag == 7:
            pool[i] = ("class", struct.unpack(">H", data[off + 1:off + 3])[0])
            off += 3
        elif tag == 8:
            off += 3
        elif tag in (9, 10, 11):
            pool[i] = ("ref", struct.unpack(">HH", data[off + 1:off + 5]))
            off += 5
        elif tag == 12:
            pool[i] = ("nat", struct.unpack(">HH", data[off + 1:off + 5]))
            off += 5
        else:
            raise ValueError(f"bad tag {tag} at {off}")
        i += 1
    return pool


def refs(path):
    pool = parse_pool(path.read_bytes())
    utf = lambda k: pool[k][1]
    out = set()
    for e in pool:
        if e and e[0] == "ref":
            cls = utf(pool[e[1][0]][1])
            name_k, desc_k = pool[e[1][1]][1]
            out.add((cls, utf(name_k) + utf(desc_k)))
        elif e and e[0] == "class":
            out.add((utf(e[1]), None))
    return out


def main():
    root = Path(sys.argv[1])
    own = {p.relative_to(root).with_suffix("").as_posix() for p in root.rglob("*.class")}
    api = defaultdict(set)
    for p in sorted(root.rglob("*.class")):
        for cls, member in refs(p):
            if cls.lstrip("[L").rstrip(";") in own:
                continue
            if member:
                api[cls].add(member)
            else:
                api.setdefault(cls, set())
    for cls in sorted(api):
        print(cls)
        for m in sorted(api[cls]):
            print("   ", m)


if __name__ == "__main__":
    main()

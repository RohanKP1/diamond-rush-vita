"""List screen-geometry constants (128 / 64 / 144 / 139 / 160 ...) per method in java-src.

Helps audit the widescreen patch: python -I tools/screen_consts.py java-src/class_9.java [method ...]
"""
import re
import sys

CONSTS = re.compile(r"(?<![\w.])(128|64|144|139|112|120|127|124|136|160|96)(?![\w.L])")
METHOD = re.compile(r"^    (?:public |private |protected )?(?:static )?(?:final )?[\w\[\]]+ (method_\d+|paint|run)\(")


def main():
    path = sys.argv[1]
    only = set(sys.argv[2:])
    current = None
    for n, line in enumerate(open(path, encoding="utf-8"), 1):
        m = METHOD.match(line)
        if m:
            current = m.group(1)
        if only and current not in only:
            continue
        code = line.split("//")[0]
        if CONSTS.search(code) and "field_" not in code.split("=")[0][-3:]:
            print(f"{n:6d} {current or '-':12s} {code.strip()[:150]}")


if __name__ == "__main__":
    main()

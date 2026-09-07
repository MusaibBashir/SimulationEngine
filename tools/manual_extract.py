"""
tools/manual_extract.py  --  read the engine's public API out of include/*.hpp.

The reference manual's signatures are EXTRACTED, never retyped, so a signature
printed in the PDF cannot drift from the one the compiler sees. Only the prose
is authored by hand, in manual_data.py.

This is a pragmatic scanner, not a C++ parser: it strips comments, tracks brace
depth and access sections, and takes anything that looks like a declaration.
That is enough for headers written in this project's style, and its failure mode
is visible -- a missing member shows up as a gap in the manual, and manual.py
prints the ones nobody wrote prose for.
"""
import io
import json
import os
import re
import sys

TYPE_RE = re.compile(r'^\s*(class|struct|enum\s+class|enum)\s+(\w+)')
MEMBER_RE = re.compile(
    r'^\s{0,8}[\w~][\w\s\*&:<>,~\[\]]*\([^;{]*\)\s*(const)?\s*(noexcept)?\s*'
    r'(override)?\s*(=\s*0)?\s*[;{]')

SKIP_NAMES = {"if", "for", "while", "switch", "return", "throw", "else",
              "catch", "assert"}


def strip_comments(lines):
    out, in_block = [], False
    for ln in lines:
        s = ln
        if in_block:
            if "*/" in s:
                s = s.split("*/", 1)[1]
                in_block = False
            else:
                out.append("")
                continue
        if "/*" in s:
            before, rest = s.split("/*", 1)
            if "*/" in rest:
                s = before + rest.split("*/", 1)[1]
            else:
                s = before
                in_block = True
        out.append(re.sub(r'//.*$', '', s))
    return out


def parse_header(path):
    raw = io.open(path, encoding="utf-8", errors="replace").read().split("\n")
    lines = strip_comments(raw)
    types, current, depth_at_open = [], None, None
    depth, access = 0, "private"

    for i, ln in enumerate(lines):
        m = TYPE_RE.match(ln)
        opens, closes = ln.count("{"), ln.count("}")

        if m and current is None:
            kind, name = m.group(1), m.group(2)
            if ln.rstrip().endswith(";"):
                # Either a forward declaration, or a whole enum on one line --
                # `enum class AssignTarget { Attribute, Variable, EntityType };`
                # is the common spelling in this codebase and must not be
                # mistaken for the former.
                if kind.startswith("enum") and "{" in ln:
                    body = ln.split("{", 1)[1].split("}", 1)[0]
                    types.append({"kind": "enum", "name": name, "line": i + 1,
                                  "bases": "", "members": [],
                                  "enumerators": [t.strip() for t in body.split(",")
                                                  if t.strip()]})
                depth += opens - closes
                continue
            head = ln.split(":", 1)
            bases = ""
            if len(head) > 1 and "::" not in head[0]:
                bases = head[1].replace("{", "").strip()
            current = {"kind": kind.replace("enum class", "enum"), "name": name,
                       "line": i + 1, "bases": bases, "members": [],
                       "enumerators": []}
            access = "private" if kind == "class" else "public"
            depth_at_open = depth

        elif current is not None:
            stripped = ln.strip()
            if stripped.startswith("public:"):
                access = "public"
            elif stripped.startswith("private:"):
                access = "private"
            elif stripped.startswith("protected:"):
                access = "protected"
            elif current["kind"] == "enum":
                for tok in re.findall(r'\b([A-Z]\w*)\b', stripped):
                    if tok not in current["enumerators"]:
                        current["enumerators"].append(tok)
            elif access == "public" and MEMBER_RE.match(ln):
                sig = re.sub(r'\s+', ' ', stripped.split("{")[0].rstrip(" ;").strip())
                first = re.match(r'^[\w~]+', sig)
                if sig and (not first or first.group(0) not in SKIP_NAMES):
                    if sig not in current["members"]:
                        current["members"].append(sig)

        depth += opens - closes
        if current is not None and closes and depth <= depth_at_open:
            types.append(current)
            current, depth_at_open = None, None

    if current:
        types.append(current)
    return types


def parse_headers(include_dir):
    """{header filename: [type records]} for every .hpp in include_dir."""
    out = {}
    for fn in sorted(os.listdir(include_dir)):
        if not fn.endswith(".hpp"):
            continue
        ts = parse_header(os.path.join(include_dir, fn))
        if ts:
            out[fn] = ts
    return out


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    api = parse_headers(os.path.join(os.path.dirname(here), "include"))
    types = sum(len(v) for v in api.values())
    members = sum(len(t["members"]) for v in api.values() for t in v)
    print(f"headers {len(api)}  types {types}  public members {members}")
    if len(sys.argv) > 1:
        io.open(sys.argv[1], "w", encoding="utf-8").write(json.dumps(api, indent=1))

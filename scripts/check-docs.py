#!/usr/bin/env python3
"""Check that SimpleGain's documentation hangs together.

Run it from anywhere:   python3 scripts/check-docs.py
Exit code 0 = all good, 1 = problems found (so CI can use it).

It checks that:
  1. every markdown link, and every #anchor in one, resolves
  2. every referenced image exists, and none in documentation/images/ is orphaned
  3. every "documentation/<file>.md section N" pointer in Source/ and CMakeLists.txt
     names a section that really exists
  4. documentation/README.md lists every doc
  5. every doc has an "In short" block and a nav line linking all the other docs
  6. code fences start their line and pair up (a mangled fence breaks the whole page)
  7. every ```cmake block is syntactically valid CMake (CLion parses them and flags errors)

Standard library only. Anchors follow GitHub's rules: lowercase the heading, strip punctuation,
then turn each space into its own hyphen -- so an em dash between two spaced words produces a
double hyphen. That is why anchors must be generated, never hand-guessed.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOC_DIR = ROOT / "documentation"

md_files = [ROOT / "README.md", ROOT / "CLAUDE.md"] + sorted(DOC_DIR.glob("*.md"))
docs = sorted(p for p in DOC_DIR.glob("*.md") if p.name != "README.md")
problems = []


def rel(path):
    return path.relative_to(ROOT) if path.is_relative_to(ROOT) else path


def slug(heading):
    text = re.sub(r"[`*]", "", heading.lstrip("#").strip()).lower()
    return re.sub(r"[^\w\s-]", "", text).replace(" ", "-")


FENCE = r"^ {0,3}```"            # CommonMark: a code fence must START a line


def strip_code_blocks(text):
    """Remove fenced blocks. Only line-start fences count -- a ``` glued to the end of a line
    is not a fence, and treating it as one flips every later pairing (it once hid a broken link)."""
    return re.sub(FENCE + r".*?" + FENCE + r".*?$", "", text, flags=re.S | re.M)


def cmake_syntax_error(source):
    """None if `source` is syntactically valid CMake, else a short reason. Syntax only: nothing runs.

    A CMake file is a series of `command(arguments)` calls. A bare list of arguments -- say the
    middle of a juce_add_plugin(...) call -- is not valid on its own, and CLion flags it."""
    n = len(source)

    def skip_blank(i):
        while i < n and (source[i].isspace() or source[i] == "#"):
            if source[i] == "#":
                while i < n and source[i] != "\n":
                    i += 1
            else:
                i += 1
        return i

    i = skip_blank(0)
    while i < n:
        name = re.match(r"[A-Za-z_]\w*", source[i:])
        if not name:
            return f"expected a command name, found {source[i:i + 24].splitlines()[0]!r}"
        i = skip_blank(i + name.end())
        if i >= n or source[i] != "(":
            return f"expected '(' after {name.group()!r}"
        depth, i = 1, i + 1
        while i < n and depth:
            char = source[i]
            if char == '"':                      # quoted argument, with \ escapes
                i += 1
                while i < n and source[i] != '"':
                    i += 2 if source[i] == "\\" else 1
                if i >= n:
                    return "unterminated quoted argument"
            elif char == "#":                    # comment to end of line
                while i < n and source[i] != "\n":
                    i += 1
                continue
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            i += 1
        if depth:
            return f"missing ')' to close {name.group()}("
        i = skip_blank(i)
    return None


anchors = {
    p.resolve(): {slug(h) for h in re.findall(r"^#+ .*$", strip_code_blocks(p.read_text()), re.M)}
    for p in md_files
}

# 1. links and anchors ---------------------------------------------------------------------
for f in md_files:
    for link in re.findall(r"\]\(([^)\s]+)\)", strip_code_blocks(f.read_text())):
        if link.startswith(("http://", "https://", "mailto:")):
            continue
        path, _, fragment = link.partition("#")
        target = (f.parent / path).resolve() if path else f.resolve()
        if not target.exists():
            problems.append(f"{rel(f)}: link target missing -> {link}")
        elif fragment and target.suffix == ".md" and fragment not in anchors.get(target, set()):
            problems.append(f"{rel(f)}: bad anchor -> {link}")

# 2. images --------------------------------------------------------------------------------
referenced = set()
for f in md_files:
    for ref in re.findall(r"(?:\]\(|src=\")([^)\"\s]+\.(?:svg|png|gif|jpg))", f.read_text()):
        if not ref.startswith(("http://", "https://")):
            referenced.add((f.parent / ref).resolve())
present = {p.resolve() for p in (DOC_DIR / "images").glob("*")}
for p in sorted(present - referenced):
    problems.append(f"orphaned image (nothing references it): {p.name}")
for p in sorted(r for r in referenced if not r.exists()):
    problems.append(f"referenced image missing: {rel(p)}")

# 3. "documentation/<file>.md section N" pointers in code ------------------------------------
sections = {
    p.name: {int(n) for n in re.findall(r"^## (\d+)\.", p.read_text(), re.M)} for p in docs
}
code_files = [p for p in (ROOT / "Source").glob("*") if p.is_file()] + [ROOT / "CMakeLists.txt"]
pointer = re.compile(r"documentation/([\w.-]+\.md)\s+sections?\s+([\d\sand,]+)")
for src in code_files:
    for m in pointer.finditer(src.read_text()):
        doc, numbers = m.group(1), [int(n) for n in re.findall(r"\d+", m.group(2))]
        if doc not in sections:
            problems.append(f"{rel(src)}: pointer to a doc that doesn't exist -> {doc}")
            continue
        for n in numbers:
            if n not in sections[doc]:
                problems.append(
                    f"{rel(src)}: {doc} has no section {n} (it has {sorted(sections[doc])})")

# 4. the docs index lists every doc ----------------------------------------------------------
listed = set(re.findall(r"\]\(([\w.-]+\.md)\)", (DOC_DIR / "README.md").read_text()))
for p in docs:
    if p.name not in listed:
        problems.append(f"documentation/README.md does not list {p.name}")

# 5. every doc: "In short" + a nav line linking every other doc -------------------------------
for p in docs:
    lines = p.read_text().split("\n")
    if "## In short" not in lines:
        problems.append(f"{p.name}: no 'In short' block")
    start = next((i for i, l in enumerate(lines) if l.startswith("**SimpleGain docs**")), None)
    if start is None:
        problems.append(f"{p.name}: no nav line")
        continue
    nav = "\n".join(lines[start:start + 4])
    for other in docs:
        if other != p and f"]({other.name})" not in nav:
            problems.append(f"{p.name}: nav does not link {other.name}")

# 6. code fences: each must start its line, and each file's fences must pair up ---------------
for f in md_files:
    text = f.read_text()
    for number, line in enumerate(text.split("\n"), start=1):
        if "```" in line and not re.match(r" {0,3}```", line) and line.count("```") % 2 == 1:
            problems.append(f"{rel(f)}:{number}: ``` is not at the start of a line, so it is not "
                            f"a code fence -- the block will render wrongly")
    if len(re.findall(FENCE, text, re.M)) % 2:
        problems.append(f"{rel(f)}: unbalanced code fences (an opening ``` has no closing one)")

# 7. cmake blocks must be valid CMake ------------------------------------------------------
for f in md_files:
    text = f.read_text()
    for block in re.finditer(r"^ {0,3}```cmake\n(.*?)^ {0,3}```", text, re.S | re.M):
        error = cmake_syntax_error(block.group(1))
        if error:
            line = text[:block.start()].count("\n") + 1
            problems.append(f"{rel(f)}:{line}: ```cmake block is not valid CMake ({error}) -- "
                            f"wrap argument fragments in their command, e.g. juce_add_plugin(...)")

# report ---------------------------------------------------------------------------------------
print(f"checked {len(md_files)} markdown files, {len(docs)} docs, {len(present)} images")
if problems:
    print(f"\n{len(problems)} PROBLEM(S):")
    for item in problems:
        print(f"  - {item}")
    sys.exit(1)
print("ALL CHECKS PASS")

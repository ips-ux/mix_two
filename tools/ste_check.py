#!/usr/bin/env python3
# AI-generated (Claude Code, 2026-11-18). Start of AI-generated text.
"""Advisory check of user-facing text against the Mix Two writing rules
(ASD-STE100 Simplified Technical English).

Usage:

    python tools/ste_check.py [--strict] [PATH ...]

Checks: sentence length, words to avoid, exclamation marks, and the names in
tools/ste_terms.txt. The file type decides which text is read:
.md (prose), .xml under res/controllers/ (<description> text and the label
of <option> and <group>), .ui (every <string> without notr="true"), and
.cpp/.h (the first string of tr() calls).
"""

import argparse
import functools
import os
import re
import sys
from xml.parsers import expat

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TERMS_FILE = os.path.join(ROOT, "tools", "ste_terms.txt")
DEFAULT_PATHS = [
    os.path.join(ROOT, "USER_changelog.md"),
    os.path.join(ROOT, "res", "controllers", "Pioneer-DDJ-800.midi.xml"),
]
MAX_WORDS = 25
RULES = ("length", "avoid", "exclaim", "term")

# (pattern, advice). Each pattern matches whole words, case-insensitive.
AVOID = [
    (r"press(?:es|ed|ing)?", 'use "push"'),
    (r"ensur(?:e|es|ed|ing)", 'use "make sure"'),
    (r"launch(?:es|ed|ing)?", 'use "start"'),
    (r"(?:(?:to|will)\s+display|displays|displayed|displaying)", 'use "show"'),
    (r"utili[sz](?:e|es|ed|ing)", 'use "use"'),
    (r"should", "state the fact or give the instruction"),
    (r"simply|just|please|basically|easily", "remove it"),
]
AVOID_RX = [
    (re.compile(r"(?<!\w)(?:%s)(?!\w)" % pat, re.I), advice)
    for pat, advice in AVOID
]

WORD = re.compile(r"[^\W_]+")
SENTENCE_END = re.compile(r"[.?!]+[\"')\]]*(?=\s|$)")
ABBREVIATIONS = ("e.g", "i.e")
BLANK_LINE = re.compile(r"\n[ \t]*\n")
TAG = re.compile(r"<[^>]*>")


# Term list ---------------------------------------------------------------


def load_terms(path):
    """Return (terms, wrong) from the term list: a list and a dict."""
    terms, wrong = [], {}
    with open(path, encoding="utf-8-sig") as handle:
        for number, raw in enumerate(handle, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if "->" in line:
                bad, good = (part.strip() for part in line.split("->", 1))
                if not bad or not good:
                    raise ValueError(
                        "%s:%d: need 'wrong -> right'" % (path, number)
                    )
                wrong[bad] = good
            else:
                terms.append(line)
    return terms, wrong


@functools.lru_cache(maxsize=None)
def _term_regex(terms):
    if not terms:
        return None
    parts = [
        r"\s+".join(re.escape(word) for word in term.split())
        for term in sorted(terms, key=len, reverse=True)
    ]
    return re.compile(r"(?<!\w)(?:%s)(?!\w)" % "|".join(parts))


def _term_spans(text, terms):
    rx = _term_regex(tuple(terms))
    return [m.span() for m in rx.finditer(text)] if rx else []


# Sentences and rules -----------------------------------------------------


def split_sentences(text, terms=()):
    """Return (offset, sentence) pairs. A term is never split."""
    masked = list(text)
    for start, end in _term_spans(text, terms):
        for i in range(start, end):
            if masked[i] in ".?!":
                masked[i] = "_"
    masked = "".join(masked)
    pieces, start = [], 0
    for match in SENTENCE_END.finditer(masked):
        if masked[: match.start()].lower().endswith(ABBREVIATIONS):
            continue
        pieces.append((start, match.end()))
        start = match.end()
    pieces.append((start, len(text)))
    result = []
    for begin, end in pieces:
        piece = text[begin:end]
        if piece.strip():
            result.append(
                (begin + len(piece) - len(piece.lstrip()), piece.strip())
            )
    return result


def check_unit(line, text, terms, wrong):
    """Check one block of text that starts on `line`.

    Return (line, rule, message) tuples. The line is the line where the
    sentence starts.
    """
    spans = _term_spans(text, terms)
    found = []
    for offset, sentence in split_sentences(text, terms):
        at = line + text.count("\n", 0, offset)
        words = len(WORD.findall(sentence))
        if words > MAX_WORDS:
            found.append(
                (at, "length", "%d words (maximum %d)" % (words, MAX_WORDS))
            )
        for rx, advice in AVOID_RX:
            for m in rx.finditer(sentence):
                lo, hi = offset + m.start(), offset + m.end()
                if any(lo < b and a < hi for a, b in spans):
                    continue
                word = " ".join(m.group().lower().split())
                found.append((at, "avoid", '"%s": %s' % (word, advice)))
        if "!" in sentence:
            found.append((at, "exclaim", "exclamation mark"))
        for bad, good in wrong.items():
            rx = re.compile(r"(?<!\w)%s(?!\w)" % re.escape(bad), re.I)
            for m in rx.finditer(sentence):
                found.append(
                    (at, "term", '"%s": use "%s"' % (m.group(), good))
                )
    return found


def _paragraphs(line, text):
    """Split a unit at blank lines; return (line, text) pairs."""
    result, start = [], 0
    for m in list(BLANK_LINE.finditer(text)) + [None]:
        end = m.start() if m else len(text)
        part = text[start:end]
        if part.strip():
            result.append((line + text.count("\n", 0, start), part))
        if m:
            start = m.end()
    return result


# Extractors: each returns a list of (line, text) -------------------------

FENCE = re.compile(r"^\s*(`{3,}|~{3,})")
LIST_ITEM = re.compile(r"^\s*(?:[-*+]|\d+[.)])\s+")
HEADING = re.compile(r"^\s{0,3}#{1,6}(?:\s+|$)")
SEPARATOR = re.compile(r"^\s*\|?\s*:?-+:?\s*(?:\|\s*:?-+:?\s*)*\|?\s*$")
QUOTE = re.compile(r"^\s*>+\s?")
HTML_COMMENT = re.compile(r"<!--.*?-->", re.S)
INLINE_CODE = re.compile(r"`+[^`]*`+")
IMAGE = re.compile(r"!\[[^\]]*\]\([^)]*\)")
LINK = re.compile(r"\[([^\]]*)\]\([^)]*\)")
REF_LINK = re.compile(r"\[([^\]]*)\]\[[^\]]*\]")
URL = re.compile(r"<?https?://[^\s>]*>?")
EMPHASIS = re.compile(r"\*+|(?<!\w)_+|_+(?!\w)")


def _clean_md(line):
    line = INLINE_CODE.sub(" ", line)
    line = IMAGE.sub("", line)
    line = LINK.sub(r"\1", line)
    line = REF_LINK.sub(r"\1", line)
    line = URL.sub("", line)
    line = TAG.sub("", line)
    return EMPHASIS.sub("", line)


def extract_md(text):
    text = HTML_COMMENT.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), text)
    units, para, fence = [], [], None

    def flush():
        if para:
            units.append((para[0][0], "\n".join(s for _n, s in para)))
            del para[:]

    for number, raw in enumerate(text.replace("\r", "").split("\n"), 1):
        match = FENCE.match(raw)
        if fence:
            if match and match.group(1)[0] == fence[0]:
                if len(match.group(1)) >= len(fence):
                    fence = None
            continue
        if match:
            flush()
            fence = match.group(1)
            continue
        line = QUOTE.sub("", raw)
        if not line.strip() or SEPARATOR.match(line):
            flush()
        elif HEADING.match(line):
            flush()
            title = re.sub(r"\s+#+\s*$", "", HEADING.sub("", line))
            units.append((number, _clean_md(title)))
        elif line.lstrip().startswith("|"):
            flush()
            for cell in line.strip().strip("|").split("|"):
                units.append((number, _clean_md(cell)))
        else:
            if LIST_ITEM.match(line):
                flush()
                line = LIST_ITEM.sub("", line, count=1)
            para.append((number, _clean_md(line)))
    flush()
    return units


def _xml_units(text, attr_tags, text_tags):
    """Read (line, text): attributes of attr_tags, text of text_tags."""
    parser = expat.ParserCreate()
    units, current = [], []

    def start(tag, attrs):
        line = parser.CurrentLineNumber
        if tag in attr_tags and attrs.get(attr_tags[tag]):
            units.append((line, attrs[attr_tags[tag]]))
        if tag in text_tags and attrs.get("notr") != "true":
            current[:] = [line, []]

    def data(chunk):
        if current:
            current[1].append(chunk)

    def end(tag):
        if tag in text_tags and current:
            units.append((current[0], "".join(current[1])))
            del current[:]

    parser.StartElementHandler = start
    parser.CharacterDataHandler = data
    parser.EndElementHandler = end
    try:
        parser.Parse(text, True)
    except expat.ExpatError as exc:
        raise ValueError("cannot parse XML: %s" % exc)
    return units


def extract_xml(text):
    return _xml_units(
        text, {"option": "label", "group": "label"}, {"description"}
    )


def extract_ui(text):
    return [(n, TAG.sub("", t)) for n, t in _xml_units(text, {}, {"string"})]


TR_CALL = re.compile(
    r'(?<!\w)(?:QObject::)?tr\s*\(\s*((?:"(?:[^"\\\n]|\\.)*"\s*)+)'
)
C_LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
C_ESCAPE = re.compile(
    r"\\(x[0-9a-fA-F]+|u[0-9a-fA-F]{4}|U[0-9a-fA-F]{8}|[0-7]{1,3}|.)", re.S
)
C_SIMPLE = {"n": "\n", "t": "\t", "r": "\r", "a": "\a", "b": "\b", "f": "\f"}


def _unescape(match):
    code = match.group(1)
    try:
        if code[0] in "xuU" and len(code) > 1:
            return chr(int(code[1:], 16))
        if code[0] in "01234567":
            return chr(int(code, 8))
    except ValueError:
        return "?"
    return C_SIMPLE.get(code, code)


def extract_cpp(text):
    units = []
    for m in TR_CALL.finditer(text):
        joined = "".join(C_LITERAL.findall(m.group(1)))
        line = text.count("\n", 0, m.start()) + 1
        units.append((line, TAG.sub("", C_ESCAPE.sub(_unescape, joined))))
    return units


EXTRACTORS = {
    ".md": extract_md,
    ".xml": extract_xml,
    ".ui": extract_ui,
    ".cpp": extract_cpp,
    ".h": extract_cpp,
}


# Files and command line --------------------------------------------------


def check_file(path, terms, wrong):
    """Return the findings of one file, sorted by line."""
    extract = EXTRACTORS.get(os.path.splitext(path)[1].lower())
    if extract is None:
        raise ValueError("%s: unsupported file type" % path)
    with open(path, encoding="utf-8-sig") as handle:
        text = handle.read()
    found = []
    try:
        units = extract(text)
    except ValueError as exc:
        raise ValueError("%s: %s" % (path, exc))
    for line, unit in units:
        for at, part in _paragraphs(line, unit):
            found.extend(check_unit(at, part, terms, wrong))
    return sorted(found, key=lambda item: item[0])


def _collect(paths):
    files = []
    for path in paths:
        if os.path.isdir(path):
            for folder, dirs, names in os.walk(path):
                dirs[:] = sorted(d for d in dirs if not d.startswith("."))
                for name in sorted(names):
                    full = os.path.join(folder, name)
                    ext = os.path.splitext(name)[1].lower()
                    posix = full.replace("\\", "/")
                    if ext not in EXTRACTORS:
                        continue
                    if ext == ".xml" and "res/controllers/" not in posix:
                        continue
                    files.append(full)
        elif os.path.isfile(path):
            files.append(path)
        else:
            raise OSError("%s: no such file or folder" % path)
    return files


def _show(path):
    try:
        rel = os.path.relpath(path)
    except ValueError:
        rel = path
    if rel.startswith(".."):
        rel = path
    return rel.replace("\\", "/")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Advisory check of user-facing text against the Mix Two "
        "writing rules (ASD-STE100). Rules: a sentence has 25 words or fewer "
        "(the real limit is 20 for an instruction, but the script cannot tell "
        "an instruction from a description, so it uses 25); words to avoid; "
        "no exclamation mark; names as in tools/ste_terms.txt. With no PATH, "
        "it checks USER_changelog.md and the DDJ-800 mapping XML.",
        epilog="Each finding is advisory and the exit code is 0. "
        "With --strict the exit code is 1 if there is a finding. "
        "A file that cannot be read gives exit code 2.",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="exit with 1 if there is a finding",
    )
    parser.add_argument(
        "paths",
        nargs="*",
        metavar="PATH",
        help="file (.md, .xml, .ui, .cpp, .h) or folder to check",
    )
    args = parser.parse_args(argv)
    try:
        terms, wrong = load_terms(TERMS_FILE)
        files = _collect(args.paths or DEFAULT_PATHS)
        results = [(f, check_file(f, terms, wrong)) for f in files]
    except (OSError, ValueError) as exc:
        sys.stderr.write("ste_check: error: %s\n" % exc)
        return 2
    counts = dict.fromkeys(RULES, 0)
    for path, found in results:
        for line, rule, message in found:
            counts[rule] += 1
            print("%s:%d: %s: %s" % (_show(path), line, rule, message))
    total = sum(counts.values())
    print(
        "ste_check: %d finding%s (%s) in %d file%s"
        % (
            total,
            "" if total == 1 else "s",
            ", ".join("%s %d" % (rule, counts[rule]) for rule in RULES),
            len(results),
            "" if len(results) == 1 else "s",
        )
    )
    return 1 if args.strict and total else 0


if __name__ == "__main__":
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except AttributeError:
            pass
    sys.exit(main())
# End of AI-generated text.

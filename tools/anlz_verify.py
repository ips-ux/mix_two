#!/usr/bin/env python3
"""AI-generated (Claude Code).

Independent check of the rekordbox analysis files (ANLZ0000.DAT, .EXT and
.2EX) that Mix Two writes, with a parser Mix Two did not write (pyrekordbox).

For every ANLZ0000.DAT under the folder (recursive) the script reads the .DAT
and the .EXT and .2EX next to it, and prints one line per track (the folder
name of the track, for example "P001/0000ABCD"): "OK" or "FAIL" with the first
failure. The exit code is 0 when every track is OK and 1 otherwise. Usage:

    python -I tools/anlz_verify.py <folder> [--reference <folder>]

Checks on each track:
  header     PMAI, len_header 28, len_file equal to the file size, then the
             values 1, 0x10000, 0x10000, 0. The sections fill the file.
  order      the fourcc order of the sections. Mix Two writes
             .DAT: PPTH PVBR PQTZ PWAV PWV2 PCOB PCOB
             .EXT: PPTH PWV3 PCOB PCOB PCO2 PCO2 PWV5 PWV4
             .2EX: PPTH PWV7 PWV6 PWVC
             and no PQT2 and no PSSI. With --reference, the folder of the
             reference may have a PQT2 after the second PCO2 of the .EXT (it is
             reported as "PQT2 present") and a last PSSI in the .EXT.
  lengths    len_header of each section, and len_tag against the entries it
             holds (this uses the raw bytes, not pyrekordbox).
  constants  PQTZ u2 0x80000, PWV3 and PWV7 unknown 0x960000, PWV5 unknown
             0x960305, PWAV and PWV2 unknown 0x10000, PVBR len_tag 1620,
             PWVC len_tag 20 (this uses the raw bytes: pyrekordbox fails on a
             constant that is different).
  widths     PWAV 400, PWV2 100, PWV4 1200 and PWV6 1200 entries. PWV3, PWV5
             and PWV7 have the same number of entries.
  cues       a PCO2 hot cue is also in a PCOB list (the same number, time and
             loop end). The hot cue list of the .DAT has only the numbers 1-3
             (A-C), the PCOB hot cue list of the .EXT only 4-8 (D-H), and the
             PCOB memory cue list of the .EXT is empty. No PCO2 hot cue has
             the number 0. The memory cues of the .DAT (PCOB) and of the .EXT
             (PCO2) are the same: number of cues, type, time and loop end.
  waveforms  (NOTE only) a PWV3, PWV5 or PWV7 with only zero bytes.
  paths      the PPTH sections of the three files are the same.
  pwvc       (folder only, not the reference) PWVC has the values 100 100 100,
             as Mix Two writes them.

With --reference the script runs the same checks on the second folder, a copy
of the analysis files that rekordbox wrote, and prints the section order lists
of both folders.

When pyrekordbox cannot parse a section, or reads the structure of a section
in another way than the checks expect, the track of the folder FAILs with
"pyrekordbox cannot parse <section>: <reason>". In the reference (rekordbox's
own files are the ground truth) this is only a NOTE line: the script checks
the raw section header (fourcc, len_header, len_tag) of that section, and the
other sections are still checked.

The script never writes to the folders: it only reads the files. It does not
print the PPTH text (the path of the music file).

Set up (the environment goes outside the repository):

    py -m venv <venv>
    <venv>/Scripts/python -m pip install \\
        "git+https://github.com/dylanljones/pyrekordbox@02a208837434e4c05c0a222e8fe6583e6ac0547f"

<!-- End of AI-generated text. -->
"""

import argparse
import collections
import logging
import pathlib
import re
import struct
import sys

try:
    from pyrekordbox.anlz import AnlzFile
except ImportError as exc:
    sys.exit(f"anlz_verify: {exc}. Install pyrekordbox (see the docstring).")

# pyrekordbox writes a warning for each tag that it does not know. The script
# prints its own NOTE lines.
logging.getLogger("pyrekordbox").setLevel(logging.CRITICAL)

KINDS = ("DAT", "EXT", "2EX")
FILE_HEADER_LEN = 28
SECTION_HEADER_MIN = 12

ORDER = {
    "DAT": ("PPTH", "PVBR", "PQTZ", "PWAV", "PWV2", "PCOB", "PCOB"),
    "EXT": (
        "PPTH",
        "PWV3",
        "PCOB",
        "PCOB",
        "PCO2",
        "PCO2",
        "PWV5",
        "PWV4",
    ),
    "2EX": ("PPTH", "PWV7", "PWV6", "PWVC"),
}
LEN_HEADER = {
    "PPTH": 16,
    "PVBR": 16,
    "PQTZ": 24,
    "PWAV": 20,
    "PWV2": 20,
    "PCOB": 24,
    "PWV3": 24,
    "PCO2": 20,
    "PWV5": 24,
    "PWV4": 24,
    "PWV7": 24,
    "PWV6": 20,
    "PWVC": 14,
    "PQT2": 56,
    "PSSI": 32,
}
# Sections with len_entry_bytes and len_entries: len_tag is
# len_header + len_entry_bytes * len_entries.
ENTRY_SECTIONS = {"PWV3": 1, "PWV5": 2, "PWV4": 6, "PWV7": 3, "PWV6": 3}
# The constants in the sections: (fourcc, field, offset in the section,
# value). pyrekordbox reads them as constants and fails on a different value,
# so the script reads them from the raw bytes.
CONSTANTS = (
    ("PQTZ", "u2", 16, 0x80000),
    ("PWV3", "unknown", 20, 0x960000),
    ("PWV7", "unknown", 20, 0x960000),
    ("PWV5", "unknown", 20, 0x960305),
    ("PWAV", "unknown", 16, 0x10000),
    ("PWV2", "unknown", 16, 0x10000),
)
PVBR_LEN_TAG = 1620
PWVC_LEN_TAG = 20
PVBR_INDEX_ENTRIES = 400
# The number of entries of the preview pictures.
PREVIEW_WIDTHS = {"PWAV": 400, "PWV2": 100, "PWV4": 1200, "PWV6": 1200}
FILE_HEADER_VALUES = (1, 0x10000, 0x10000, 0)
MIX_TWO_PWVC = [100, 100, 100]
# A bytes literal in the text of an exception: b'...' or b"...".
BYTES_LITERAL = re.compile(r"""b'(?:[^'\\]|\\.)*'|b"(?:[^"\\]|\\.)*\"""")
DAT_HOT_CUES = (1, 2, 3)
EXT_HOT_CUES = (4, 5, 6, 7, 8)
HOT_CUE_LIST = "hotcue"
MEMORY_CUE_LIST = "memory"
LIST_ORDER = [HOT_CUE_LIST, MEMORY_CUE_LIST]


class CheckFailed(Exception):
    """A check found a problem. The text is the reason."""


class Section:
    """One section as it lies in the bytes of a file. `tag` is the parsed
    pyrekordbox tag, or None when pyrekordbox could not parse it."""

    def __init__(self, fourcc, offset, len_header, len_tag, data):
        self.fourcc = fourcc
        self.offset = offset
        self.len_header = len_header
        self.len_tag = len_tag
        self.data = data
        self.tag = None


class AnlzBytes:
    """The bytes of one file of a track, with its sections."""

    def __init__(self, kind, data, strict):
        self.kind = kind
        self.data = data
        # True for the folder of Mix Two: a gap of pyrekordbox is a failure.
        # False for the reference: a gap of pyrekordbox is only a note.
        self.strict = strict
        self.sections = []
        self.notes = []

    def parser_problem(self, text):
        """pyrekordbox cannot read something. This fails the track, except in
        the reference, where it is a note (the files of rekordbox are the
        ground truth)."""
        if self.strict:
            raise CheckFailed(f"{self.kind}: pyrekordbox cannot parse {text}")
        self.notes.append(
            f"pyrekordbox cannot parse {text} in {self.kind}; "
            "the raw header is checked"
        )

    @property
    def fourccs(self):
        return [s.fourcc for s in self.sections]

    def of(self, fourcc):
        return [s for s in self.sections if s.fourcc == fourcc]


def u32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def read_raw(kind, data, strict):
    """Check the file header and split the file into sections (raw bytes)."""
    if len(data) < FILE_HEADER_LEN:
        raise CheckFailed(f"{kind}: the file has only {len(data)} bytes")
    if data[:4] != b"PMAI":
        raise CheckFailed(f"{kind}: the file does not start with PMAI")
    len_header = u32(data, 4)
    len_file = u32(data, 8)
    if len_header != FILE_HEADER_LEN:
        raise CheckFailed(f"{kind}: len_header is {len_header}, expected 28")
    if len_file != len(data):
        raise CheckFailed(
            f"{kind}: len_file is {len_file}, the file has {len(data)} bytes"
        )
    values = tuple(u32(data, 12 + 4 * i) for i in range(4))
    if values != FILE_HEADER_VALUES:
        raise CheckFailed(
            f"{kind}: header values are "
            f"{[hex(v) for v in values]}, expected "
            f"{[hex(v) for v in FILE_HEADER_VALUES]}"
        )
    result = AnlzBytes(kind, data, strict)
    offset = len_header
    while offset < len(data):
        if offset + SECTION_HEADER_MIN > len(data):
            raise CheckFailed(
                f"{kind}: {len(data) - offset} bytes at the end of the file "
                "are not a section"
            )
        fourcc = data[offset : offset + 4].decode("latin1")
        section_len_header = u32(data, offset + 4)
        len_tag = u32(data, offset + 8)
        if len_tag < SECTION_HEADER_MIN or offset + len_tag > len(data):
            raise CheckFailed(
                f"{kind}: {fourcc} has len_tag {len_tag}, "
                "this is beyond the end of the file"
            )
        result.sections.append(
            Section(
                fourcc,
                offset,
                section_len_header,
                len_tag,
                data[offset : offset + len_tag],
            )
        )
        offset += len_tag
    return result


def attach_tags(parsed_file):
    """Parse the sections with pyrekordbox. The whole file is parsed first.
    When that fails, each section is parsed alone (with a file header made for
    it), to name the sections that pyrekordbox cannot parse."""
    data = parsed_file.data
    try:
        tags = AnlzFile.parse(data).tags
    except Exception as exc:
        tags = None
        whole_error = f"{type(exc).__name__}: {first_line(exc)}"
    if tags is not None and len(tags) == len(parsed_file.sections):
        for section, tag in zip(parsed_file.sections, tags):
            section.tag = tag
        return
    if tags is not None:
        whole_error = (
            f"{len(tags)} tags for {len(parsed_file.sections)} sections"
        )
    unparsed = False
    for section in parsed_file.sections:
        header = bytearray(data[:FILE_HEADER_LEN])
        header[8:12] = struct.pack(">I", FILE_HEADER_LEN + section.len_tag)
        try:
            section.tag = AnlzFile.parse(bytes(header) + section.data).tags[0]
        except Exception as exc:
            unparsed = True
            reason = type(exc).__name__
            if section.fourcc != "PPTH":
                reason += f": {first_line(exc)}"
            parsed_file.parser_problem(f"{section.fourcc}: {reason}")
    if not unparsed:
        # Each section is fine alone, but not the whole file.
        parsed_file.parser_problem(f"the whole file: {whole_error}")


def first_line(exc):
    """The first line of an exception text. A bytes value is left out: the
    text of a failed PPTH holds the path of the music file."""
    lines = str(exc).splitlines()
    return BYTES_LITERAL.sub("<bytes>", lines[0]) if lines else ""


def content_of(section):
    """The pyrekordbox content of a section, or None when pyrekordbox could
    not parse the section."""
    if section.tag is None:
        return None
    return section.tag.struct.content


def with_structure(parsed_file, section, check, *args):
    """Run a check that reads the pyrekordbox structure of a section. When the
    structure is not the one that the check expects, this is a failure (a note
    in the reference) and the check is skipped. CheckFailed passes through."""
    content = content_of(section)
    if content is None:
        return None
    try:
        return check(content, *args)
    except (KeyError, AttributeError, TypeError, IndexError) as exc:
        parsed_file.parser_problem(
            f"the structure of {section.fourcc} "
            f"({type(exc).__name__}: {first_line(exc)})"
        )
        return None


def check_order(parsed_file, is_reference):
    """The fourcc order. Returns True when the file has a PQT2."""
    expected = list(ORDER[parsed_file.kind])
    actual = parsed_file.fourccs
    has_pqt2 = False
    if is_reference and parsed_file.kind == "EXT":
        # rekordbox writes PQT2 after the second PCO2, and PSSI at the end.
        if "PQT2" in actual:
            has_pqt2 = True
            actual = [f for f in actual if f != "PQT2"]
        if actual and actual[-1] == "PSSI":
            actual = actual[:-1]
    if actual != expected:
        raise CheckFailed(
            f"{parsed_file.kind}: the sections are {' '.join(actual)}, "
            f"expected {' '.join(expected)}"
        )
    if has_pqt2:
        # Only after the second PCO2, as the order of the file shows.
        names = parsed_file.fourccs
        if names.index("PQT2") != names.index("PCO2") + 2:
            raise CheckFailed("EXT: PQT2 is not after the second PCO2")
    return has_pqt2


def check_lengths(parsed_file):
    """len_header of each section, and len_tag against the entries."""
    kind = parsed_file.kind
    for section in parsed_file.sections:
        expected = LEN_HEADER.get(section.fourcc)
        if expected is not None and section.len_header != expected:
            raise CheckFailed(
                f"{kind}: {section.fourcc} has len_header "
                f"{section.len_header}, expected {expected}"
            )
        if section.fourcc in ENTRY_SECTIONS:
            entry_bytes = u32(section.data, 12)
            entries = u32(section.data, 16)
            if entry_bytes != ENTRY_SECTIONS[section.fourcc]:
                raise CheckFailed(
                    f"{kind}: {section.fourcc} entries have {entry_bytes} "
                    f"bytes, expected {ENTRY_SECTIONS[section.fourcc]}"
                )
            size = section.len_header + entry_bytes * entries
            if section.len_tag != size:
                raise CheckFailed(
                    f"{kind}: {section.fourcc} has len_tag "
                    f"{section.len_tag}, its {entries} entries need {size}"
                )
        if section.fourcc == "PPTH":
            len_path = u32(section.data, 12)
            if section.len_tag != section.len_header + len_path:
                raise CheckFailed(
                    f"{kind}: PPTH has len_tag {section.len_tag}, "
                    f"its path has {len_path} bytes"
                )
            if len_path % 2:
                raise CheckFailed(f"{kind}: PPTH path has an odd length")
        if section.fourcc in ("PWAV", "PWV2"):
            # PWAV and PWV2: len_preview, a constant, then the bytes.
            width = u32(section.data, 12)
            if section.len_tag != section.len_header + width:
                raise CheckFailed(
                    f"{kind}: {section.fourcc} has len_tag "
                    f"{section.len_tag}, its picture has {width} bytes"
                )
        if section.fourcc == "PQTZ":
            count = u32(section.data, 20)
            if section.len_tag != section.len_header + 8 * count:
                raise CheckFailed(
                    f"{kind}: PQTZ has len_tag {section.len_tag}, "
                    f"its {count} beats need {section.len_header + 8 * count}"
                )
        if section.fourcc == "PVBR" and section.len_tag != PVBR_LEN_TAG:
            raise CheckFailed(
                f"{kind}: PVBR has len_tag {section.len_tag}, "
                f"expected {PVBR_LEN_TAG}"
            )
        if section.fourcc == "PWVC" and section.len_tag != PWVC_LEN_TAG:
            raise CheckFailed(
                f"{kind}: PWVC has len_tag {section.len_tag}, "
                f"expected {PWVC_LEN_TAG}"
            )


def entry_count(content):
    """The number of entries that pyrekordbox read."""
    return len(content["entries"])


def check_constants(parsed_file):
    kind = parsed_file.kind
    for fourcc, field, offset, expected in CONSTANTS:
        for section in parsed_file.of(fourcc):
            value = u32(section.data, offset)
            if value != expected:
                raise CheckFailed(
                    f"{kind}: {fourcc} {field} is {value:#x}, "
                    f"expected {expected:#x}"
                )


def check_beats(content):
    if content["entry_count"] != entry_count(content):
        raise CheckFailed(
            f"entry_count is {content['entry_count']}, "
            f"pyrekordbox read {entry_count(content)} beats"
        )
    previous = -1
    for beat in content["entries"]:
        if not 1 <= beat["beat"] <= 4:
            raise CheckFailed(f"a beat has the number {beat['beat']}")
        if beat["time"] < previous:
            raise CheckFailed("the beat times do not go up")
        previous = beat["time"]


def raw_width(section):
    """The number of entries of a picture, from the raw bytes: PWAV and PWV2
    give the bytes (one byte per entry), PWV4 and PWV6 the number."""
    if section.fourcc in ("PWAV", "PWV2"):
        return u32(section.data, 12)
    return u32(section.data, 16)


def check_widths(parsed_file):
    """Fixed widths of the previews, and the beats and the index."""
    kind = parsed_file.kind
    for fourcc, width in PREVIEW_WIDTHS.items():
        for section in parsed_file.of(fourcc):
            if raw_width(section) != width:
                raise CheckFailed(
                    f"{kind}: {fourcc} has {raw_width(section)} entries, "
                    f"expected {width}"
                )
            if fourcc in ("PWAV", "PWV2"):
                count = with_structure(
                    parsed_file, section, lambda c: len(c["entries"])
                )
                if count is not None and count != width:
                    raise CheckFailed(
                        f"{kind}: pyrekordbox read {count} {fourcc} "
                        f"entries, expected {width}"
                    )
    for section in parsed_file.of("PQTZ"):
        try:
            with_structure(parsed_file, section, check_beats)
        except CheckFailed as exc:
            raise CheckFailed(f"{kind}: PQTZ {exc}") from None
    for section in parsed_file.of("PVBR"):
        count = with_structure(parsed_file, section, lambda c: len(c["idx"]))
        if count is not None and count != PVBR_INDEX_ENTRIES:
            raise CheckFailed(f"{kind}: PVBR has {count} index entries")


def detail_entries(files):
    """PWV3, PWV5 and PWV7 must have the same number of entries (from the
    raw bytes)."""
    counts = {}
    for kind, fourcc in (("EXT", "PWV3"), ("EXT", "PWV5"), ("2EX", "PWV7")):
        sections = files[kind].of(fourcc)
        counts[fourcc] = u32(sections[0].data, 16)
    if len(set(counts.values())) != 1:
        raise CheckFailed(
            "PWV3, PWV5 and PWV7 have different entry counts: "
            + ", ".join(f"{k} {v}" for k, v in counts.items())
        )


def cue_entries(content):
    """(number, time, loop end, type) of the cues in a PCOB or PCO2 content.
    The type is 1 for a cue and 2 for a loop (PCOB: an enum, PCO2: a number).
    """
    return [
        (e["hot_cue"], e["time"], e["loop_time"], int(e["type"]))
        for e in content["entries"]
    ]


def cue_list_type(content):
    """'hotcue' or 'memory' (PCOB: cue_type; PCO2: type)."""
    value = content["cue_type"] if "cue_type" in content else content["type"]
    return str(value)


def check_cue_counts(parsed_file):
    kind = parsed_file.kind

    def check(content, section):
        if content["count"] != entry_count(content):
            raise CheckFailed(
                f"count is {content['count']}, "
                f"pyrekordbox read {entry_count(content)} cues"
            )
        if section.fourcc == "PCOB":
            size = section.len_header + 56 * entry_count(content)
        else:
            size = section.len_header + sum(
                e["len_entry"] for e in content["entries"]
            )
        if section.len_tag != size:
            raise CheckFailed(
                f"len_tag is {section.len_tag}, the cues need {size}"
            )

    for fourcc in ("PCOB", "PCO2"):
        for section in parsed_file.of(fourcc):
            try:
                with_structure(parsed_file, section, check, section)
            except CheckFailed as exc:
                raise CheckFailed(f"{kind}: {fourcc} {exc}") from None
    # A hot cue list comes first, then the memory cue list.
    for fourcc in ("PCOB", "PCO2"):
        types = []
        for section in parsed_file.of(fourcc):
            types.append(with_structure(parsed_file, section, cue_list_type))
        if types and None not in types and types != LIST_ORDER:
            raise CheckFailed(
                f"{kind}: the {fourcc} lists are {types}, "
                f"expected {LIST_ORDER}"
            )


def describe_cue(cue):
    """The text of a cue tuple for a message."""
    return (
        f"number {cue[0]}, type {cue[3]}, time {cue[1]}, "
        f"loop end {cue[2]:#x}"
    )


def check_memory_cues(lists):
    """The memory cues of the .DAT (PCOB) and of the .EXT (PCO2) are the
    same: the number of cues, and the type, time and loop end of each. The
    memory list of the PCOB in the .EXT is empty."""
    ext_memory_pcob = lists.get(("EXT", "PCOB", MEMORY_CUE_LIST))
    if ext_memory_pcob:
        raise CheckFailed(
            f"EXT: the PCOB memory cue list has {len(ext_memory_pcob)} "
            "cues, expected none"
        )
    dat_memory = lists.get(("DAT", "PCOB", MEMORY_CUE_LIST))
    ext_memory = lists.get(("EXT", "PCO2", MEMORY_CUE_LIST))
    if dat_memory is None or ext_memory is None:
        return
    if len(dat_memory) != len(ext_memory):
        raise CheckFailed(
            f"DAT: the PCOB memory cue list has {len(dat_memory)} cues, "
            f"EXT: the PCO2 memory cue list has {len(ext_memory)}"
        )
    for index, (dat_cue, ext_cue) in enumerate(zip(dat_memory, ext_memory)):
        if dat_cue != ext_cue:
            raise CheckFailed(
                f"memory cue {index + 1} differs: DAT PCOB has "
                f"{describe_cue(dat_cue)}, EXT PCO2 has "
                f"{describe_cue(ext_cue)}"
            )


def check_cues(files):
    """The cue checks of the whole track."""
    dat, ext = files["DAT"], files["EXT"]
    lists = {}
    for parsed_file in (dat, ext):
        for fourcc in ("PCOB", "PCO2"):
            for section in parsed_file.of(fourcc):
                content = content_of(section)
                if content is None:
                    continue
                try:
                    kind_of_list = cue_list_type(content)
                    cues = cue_entries(content)
                except (
                    KeyError,
                    AttributeError,
                    TypeError,
                    ValueError,
                ) as exc:
                    parsed_file.parser_problem(
                        f"the structure of {fourcc} "
                        f"({type(exc).__name__}: {first_line(exc)})"
                    )
                    return
                lists[(parsed_file.kind, fourcc, kind_of_list)] = cues
    dat_hot = lists.get(("DAT", "PCOB", HOT_CUE_LIST))
    if dat_hot is not None:
        wrong = sorted({c[0] for c in dat_hot} - set(DAT_HOT_CUES))
        if wrong:
            raise CheckFailed(
                f"DAT: the hot cue list has the cue numbers {wrong}, "
                "expected only 1-3 (A-C)"
            )
    ext_hot_pcob = lists.get(("EXT", "PCOB", HOT_CUE_LIST))
    if ext_hot_pcob is not None:
        wrong = sorted({c[0] for c in ext_hot_pcob} - set(EXT_HOT_CUES))
        if wrong:
            raise CheckFailed(
                f"EXT: the PCOB hot cue list has the cue numbers {wrong}, "
                "expected only 4-8 (D-H)"
            )
    check_memory_cues(lists)
    ext_hot_cue2 = lists.get(("EXT", "PCO2", HOT_CUE_LIST))
    if ext_hot_cue2 is None:
        return
    in_pcob = {c[:3] for c in dat_hot or []} | {
        c[:3] for c in ext_hot_pcob or []
    }
    for cue in ext_hot_cue2:
        if cue[0] == 0:
            raise CheckFailed(
                f"EXT: the PCO2 hot cue list has a cue numbered 0 "
                f"(time {cue[1]}), expected 1-8"
            )
        if cue[:3] not in in_pcob:
            raise CheckFailed(
                f"EXT: PCO2 hot cue {cue[0]} (time {cue[1]}, "
                f"loop end {cue[2]:#x}) is in no PCOB list"
            )


def note_silent_waveforms(files):
    """A NOTE for a detail waveform with only zero bytes: it is a valid file,
    but the player shows silence."""
    for kind, fourcc in (("EXT", "PWV3"), ("EXT", "PWV5"), ("2EX", "PWV7")):
        for section in files[kind].of(fourcc):
            payload = section.data[section.len_header :]
            if payload and not any(payload):
                files[kind].notes.append(
                    f"{fourcc} in {kind} has only zero bytes "
                    "(the waveform is silent)"
                )


def check_paths(files):
    paths = {
        kind: files[kind].of("PPTH")[0].data
        for kind in KINDS
        if files[kind].of("PPTH")
    }
    if len(set(paths.values())) > 1:
        raise CheckFailed("the PPTH sections of the three files differ")


def check_pwvc(files):
    """Mix Two writes 100 100 100 (not checked for the reference)."""
    for section in files["2EX"].of("PWVC"):

        def check(content):
            values = [int(v) for v in content["data"]]
            if values != MIX_TWO_PWVC:
                raise CheckFailed(
                    f"2EX: PWVC has the values {values}, "
                    f"expected {MIX_TWO_PWVC}"
                )

        with_structure(files["2EX"], section, check)


def check_track(folder, is_reference):
    """Run every check on the three files of one track. Returns
    (orders, notes, pqt2) or raises CheckFailed with the first failure."""
    files = {}
    strict = not is_reference
    for kind in KINDS:
        path = folder / f"ANLZ0000.{kind}"
        if not path.is_file():
            raise CheckFailed(f"{path.name} does not exist")
        with open(path, "rb") as handle:
            files[kind] = read_raw(kind, handle.read(), strict)
    pqt2 = False
    for parsed_file in files.values():
        pqt2 = check_order(parsed_file, is_reference) or pqt2
        check_lengths(parsed_file)
        check_constants(parsed_file)
    for parsed_file in files.values():
        attach_tags(parsed_file)
        check_widths(parsed_file)
        check_cue_counts(parsed_file)
    detail_entries(files)
    check_cues(files)
    note_silent_waveforms(files)
    check_paths(files)
    if not is_reference:
        check_pwvc(files)
    orders = {kind: tuple(files[kind].fourccs) for kind in KINDS}
    notes = [note for kind in KINDS for note in files[kind].notes]
    return orders, notes, pqt2


def track_name(root, folder):
    relative = folder.relative_to(root).as_posix()
    return root.name if relative == "." else relative


def check_folder(label, root, is_reference):
    """Check every track under root. Prints the lines. Returns
    (failed, orders), orders = {kind: Counter of order tuples}."""
    print(f"{label}: {root}")
    orders = {kind: collections.Counter() for kind in KINDS}
    dat_files = sorted(root.rglob("ANLZ0000.DAT"))
    if not dat_files:
        print(f"FAIL {label}: no ANLZ0000.DAT under the folder")
        return True, orders
    failed = False
    for dat_file in dat_files:
        name = track_name(root, dat_file.parent)
        try:
            track_orders, notes, pqt2 = check_track(
                dat_file.parent, is_reference
            )
        except (CheckFailed, OSError, struct.error) as exc:
            failed = True
            print(f"FAIL {name}: {exc}")
            continue
        for kind, order in track_orders.items():
            orders[kind][order] += 1
        print(f"OK   {name}" + ("  (PQT2 present)" if pqt2 else ""))
        for note in notes:
            print(f"  NOTE {note}")
    return failed, orders


def print_orders(label, orders):
    print(f"Section order, {label}:")
    for kind in KINDS:
        for order, count in orders[kind].most_common():
            print(f"  .{kind}: {' '.join(order)}  ({count} tracks)")


def main():
    parser = argparse.ArgumentParser(
        description="Check rekordbox analysis files with pyrekordbox."
    )
    parser.add_argument(
        "folder",
        help="folder with ANLZ0000.DAT/.EXT/.2EX files (searched recursively)",
    )
    parser.add_argument(
        "--reference",
        metavar="FOLDER",
        help="folder with analysis files that rekordbox wrote "
        "(PQT2 and PSSI are accepted there)",
    )
    args = parser.parse_args()

    runs = [("folder", pathlib.Path(args.folder), False)]
    if args.reference:
        runs.append(("reference", pathlib.Path(args.reference), True))

    failed = False
    results = []
    for label, root, is_reference in runs:
        run_failed, orders = check_folder(label, root, is_reference)
        failed = failed or run_failed
        results.append((label, orders))
    if args.reference:
        for label, orders in results:
            print_orders(label, orders)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

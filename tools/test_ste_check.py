# AI-generated (Claude Code, 2026-11-18). Start of AI-generated text.
"""Unit tests for ste_check.py. Run from the repository root:

python tools/test_ste_check.py
"""

import contextlib
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ste_check  # noqa: E402

TERMS, WRONG = ste_check.load_terms(ste_check.TERMS_FILE)


def check(text):
    """Return the (rule, message) pairs found in one block of text."""
    found = ste_check.check_unit(1, text, TERMS, WRONG)
    return [(rule, msg) for _line, rule, msg in found]


def rules(text):
    return [rule for rule, _msg in check(text)]


class TempFiles(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)

    def write(self, name, content):
        path = os.path.join(self._tmp.name, name)
        with open(path, "w", encoding="utf-8", newline="") as handle:
            handle.write(content)
        return path


class SentenceLength(unittest.TestCase):
    def test_26_words_reported(self):
        text = " ".join(["word"] * 26) + "."
        self.assertEqual(rules(text), ["length"])

    def test_25_words_not_reported(self):
        text = " ".join(["word"] * 25) + "."
        self.assertEqual(rules(text), [])

    def test_term_counts_as_two_words(self):
        text = "KEY SYNC " + " ".join(["word"] * 23) + "."
        self.assertEqual(rules(text), [])
        self.assertEqual(
            rules("KEY SYNC " + " ".join(["word"] * 24) + "."), ["length"]
        )


class WordsToAvoid(unittest.TestCase):
    CASES = [
        ("Press the button.", "push"),
        ("The key was pressed.", "push"),
        ("Pressing the key is bad.", "push"),
        ("Ensure the cable is in.", "make sure"),
        ("This ensures a good mix.", "make sure"),
        ("Launch the app.", "start"),
        ("The app launches fast.", "start"),
        ("The screen displays the BPM.", "show"),
        ("The BPM is displayed.", "show"),
        ("The screen is displaying the BPM.", "show"),
        ("Use the screen to display the BPM.", "show"),
        ("The screen will display the BPM.", "show"),
        ("Utilize the knob.", "use"),
        ("You should push the knob.", "state the fact"),
        ("Simply push the knob.", "remove"),
        ("Just push the knob.", "remove"),
        ("Please push the knob.", "remove"),
        ("Basically the knob turns.", "remove"),
        ("Easily push the knob.", "remove"),
    ]

    def test_each_word_is_reported_with_the_approved_word(self):
        for text, approved in self.CASES:
            with self.subTest(text=text):
                found = check(text)
                self.assertEqual([r for r, _m in found], ["avoid"])
                self.assertIn(approved, found[0][1])

    def test_whole_words_only(self):
        self.assertEqual(rules("The express train has a justice."), [])

    def test_case_is_ignored(self):
        self.assertEqual(rules("PRESS the key."), ["avoid"])

    def test_the_display_is_not_reported(self):
        self.assertEqual(rules("The display shows the BPM."), [])
        self.assertEqual(rules("Look at the display."), [])


class ExclamationMark(unittest.TestCase):
    def test_reported(self):
        self.assertEqual(rules("Push the knob!"), ["exclaim"])

    def test_none(self):
        self.assertEqual(rules("Push the knob."), [])


class Terms(unittest.TestCase):
    def test_term_is_not_a_word_to_avoid(self):
        self.assertEqual(rules("LOAD"), [])
        self.assertEqual(rules("Push SHIFT + LOAD."), [])

    def test_wrong_name_is_reported(self):
        found = check("Mix Two reads OneLibrary.")
        self.assertEqual([r for r, _m in found], ["term"])
        self.assertIn("Device Library Plus", found[0][1])

    def test_right_name_is_not_reported(self):
        self.assertEqual(rules("Mix Two reads Device Library Plus."), [])

    def test_term_with_a_dot_is_not_split(self):
        terms = ["Mr. X"]
        sentences = ste_check.split_sentences("Ask Mr. X now. Then go.", terms)
        self.assertEqual(
            [s for _o, s in sentences], ["Ask Mr. X now.", "Then go."]
        )

    def test_terms_file_is_read(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "t.txt")
            with open(path, "w", encoding="utf-8") as handle:
                handle.write("# comment\n\nSYNC\nBad -> Good\n")
            self.assertEqual(
                ste_check.load_terms(path), (["SYNC"], {"Bad": "Good"})
            )

    def test_broken_terms_line_raises(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "t.txt")
            with open(path, "w", encoding="utf-8") as handle:
                handle.write(" -> Good\n")
            with self.assertRaises(ValueError):
                ste_check.load_terms(path)


class SentenceSplit(unittest.TestCase):
    def split(self, text):
        return [s for _o, s in ste_check.split_sentences(text, TERMS)]

    def test_decimal_and_fraction_are_one_sentence(self):
        text = "1.8 s, as on a record at 33 1/3 rpm."
        self.assertEqual(self.split(text), [text])

    def test_file_name_and_version(self):
        text = "Double-click mixtwo_windows.bat for version 2.6.1 now."
        self.assertEqual(self.split(text), [text])

    def test_abbreviation(self):
        text = "Use a key, e.g. SYNC, to start."
        self.assertEqual(self.split(text), [text])

    def test_question_and_exclamation(self):
        self.assertEqual(
            self.split("Is it on? Yes! Go."), ["Is it on?", "Yes!", "Go."]
        )


class MarkdownExtract(TempFiles):
    def test_code_comment_and_inline_code_are_not_checked(self):
        md = (
            "# Press title\n"
            "\n"
            "Plain `press` text.\n"
            "\n"
            "```\n"
            "press the key! simply\n"
            "```\n"
            "\n"
            "<!-- Press the key. -->\n"
            "Ensure it.\n"
            "\n"
            "<!--\n"
            "just a comment!\n"
            "-->\n"
            "| a | b |\n"
            "| --- | --- |\n"
            "| [link](http://press.example/please) | ok |\n"
        )
        path = self.write("a.md", md)
        found = ste_check.check_file(path, TERMS, WRONG)
        self.assertEqual(
            [(ln, rule) for ln, rule, _m in found],
            [(1, "avoid"), (10, "avoid")],
        )

    def test_paragraph_lines_are_joined_and_numbered(self):
        md = "First line of a\nsentence that goes on.\nPress it.\n"
        path = self.write("b.md", md)
        found = ste_check.check_file(path, TERMS, WRONG)
        self.assertEqual(
            [(ln, rule) for ln, rule, _m in found], [(3, "avoid")]
        )

    def test_list_items_are_separate_sentences(self):
        md = "- Push SYNC\n- Press LOAD\n"
        path = self.write("c.md", md)
        found = ste_check.check_file(path, TERMS, WRONG)
        self.assertEqual(
            [(ln, rule) for ln, rule, _m in found], [(2, "avoid")]
        )

    def test_crlf_is_tolerated(self):
        path = self.write("d.md", "One.\r\n\r\nPress it.\r\n")
        found = ste_check.check_file(path, TERMS, WRONG)
        self.assertEqual(
            [(ln, rule) for ln, rule, _m in found], [(3, "avoid")]
        )


class XmlExtract(TempFiles):
    SAMPLE = (
        '<?xml version="1.0"?>\n'
        "<root>\n"
        '  <group label="Mixer">\n'
        '    <option variable="a" label="Press the knob">\n'
        "      <description>\n"
        "        Use it.\n"
        "        Please use it.\n"
        "      </description>\n"
        "    </option>\n"
        "  </group>\n"
        "</root>\n"
    )

    def test_units_and_line_numbers(self):
        path = self.write("x.xml", self.SAMPLE)
        found = ste_check.check_file(path, TERMS, WRONG)
        self.assertEqual(
            [(ln, rule) for ln, rule, _m in found],
            [(4, "avoid"), (7, "avoid")],
        )

    def test_extract_lists_label_and_description(self):
        units = ste_check.extract_xml(self.SAMPLE)
        self.assertEqual([u[0] for u in units], [3, 4, 5])
        self.assertEqual(units[0][1], "Mixer")
        self.assertEqual(units[1][1], "Press the knob")

    def test_broken_xml_raises(self):
        path = self.write("bad.xml", "<root><a></root>")
        with self.assertRaises(ValueError):
            ste_check.check_file(path, TERMS, WRONG)


class UiExtract(TempFiles):
    SAMPLE = (
        '<ui version="4.0">\n'
        ' <widget class="QLabel">\n'
        '  <property name="text">\n'
        "   <string>Press the key.</string>\n"
        "  </property>\n"
        '  <property name="styleSheet">\n'
        '   <string notr="true">press: please;</string>\n'
        "  </property>\n"
        " </widget>\n"
        "</ui>\n"
    )

    def test_notr_is_skipped(self):
        units = ste_check.extract_ui(self.SAMPLE)
        self.assertEqual(units, [(4, "Press the key.")])

    def test_findings(self):
        path = self.write("a.ui", self.SAMPLE)
        found = ste_check.check_file(path, TERMS, WRONG)
        self.assertEqual(
            [(ln, rule) for ln, rule, _m in found], [(4, "avoid")]
        )


class CppExtract(TempFiles):
    def test_adjacent_literals_are_joined(self):
        src = 'auto s = tr("Push " "the button");\n'
        self.assertEqual(ste_check.extract_cpp(src), [(1, "Push the button")])

    def test_qobject_tr_escapes_and_line(self):
        src = (
            "int x = 1;\n"
            'auto s = QObject::tr("Say \\"hi\\"\\tnow", "context");\n'
            'auto t = other("no");\n'
        )
        self.assertEqual(ste_check.extract_cpp(src), [(2, 'Say "hi"\tnow')])

    def test_not_tr_suffix(self):
        self.assertEqual(
            ste_check.extract_cpp('x = str("a"); y = retr("b");'), []
        )

    def test_findings(self):
        path = self.write("a.cpp", 'void f() {\n  tr("Press it");\n}\n')
        found = ste_check.check_file(path, TERMS, WRONG)
        self.assertEqual(
            [(ln, rule) for ln, rule, _m in found], [(2, "avoid")]
        )


class ExitCodes(TempFiles):
    def run_main(self, *args):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            try:
                code = ste_check.main(list(args))
            except SystemExit as exc:
                code = exc.code
        return code, out.getvalue(), err.getvalue()

    def test_findings_exit_0(self):
        path = self.write("a.md", "Press it.\n")
        code, out, _err = self.run_main(path)
        self.assertEqual(code, 0)
        self.assertIn("a.md:1: avoid:", out)
        self.assertIn("1 finding", out)

    def test_strict_with_findings_exit_1(self):
        path = self.write("a.md", "Press it.\n")
        self.assertEqual(self.run_main("--strict", path)[0], 1)

    def test_strict_without_findings_exit_0(self):
        path = self.write("a.md", "Push it.\n")
        code, out, _err = self.run_main("--strict", path)
        self.assertEqual(code, 0)
        self.assertIn("0 findings", out)

    def test_missing_file_exit_2(self):
        missing = os.path.join(self._tmp.name, "missing.md")
        code, _out, err = self.run_main(missing)
        self.assertEqual(code, 2)
        self.assertIn("missing.md", err)
        self.assertEqual(self.run_main("--strict", missing)[0], 2)

    def test_unsupported_type_exit_2(self):
        path = self.write("a.txt", "Press it.\n")
        self.assertEqual(self.run_main(path)[0], 2)

    def test_folder_is_walked(self):
        self.write("a.md", "Press it.\n")
        self.write("b.txt", "Press it.\n")
        code, out, _err = self.run_main(self._tmp.name)
        self.assertEqual(code, 0)
        self.assertIn("1 finding", out)
        self.assertIn("1 file", out)


if __name__ == "__main__":
    unittest.main()
# End of AI-generated text.

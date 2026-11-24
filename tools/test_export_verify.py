# AI-generated (Claude Code). Start of AI-generated text.
"""Unit tests for export_verify.py. Run from the repository root:

python tools/test_export_verify.py
"""

import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import export_verify as ev  # noqa: E402


class FakeJudge(ev.Judge):
    def __init__(self, name, result, detail="detail", error=None):
        self.name = name
        self._result = result
        self._detail = detail
        self._error = error
        self.called = False

    def run(self, stick, reference):
        self.called = True
        if self._error:
            raise self._error
        return self._result, self._detail


def good_set(**overrides):
    results = {
        "dlp_verify": ev.PASS,
        "anlz_verify": ev.PASS,
        "rekordcrate": ev.PASS,
        "crate-digger": ev.PASS,
        "rekordbox": ev.PASS,
    }
    results.update(overrides)
    return [FakeJudge(name, result) for name, result in results.items()]


class ExportVerifyTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.stick = self._tmp.name

    def run_main(self, judges, *extra):
        out = io.StringIO()
        code = ev.main([self.stick, *extra], judges=judges, out=out)
        return code, out.getvalue()

    def test_all_pass(self):
        code, text = self.run_main(good_set())
        self.assertEqual(code, 0)
        lines = text.splitlines()
        # header + 5 judges + last line
        self.assertEqual(len(lines), 7)
        for name in ("dlp_verify", "anlz_verify", "rekordcrate"):
            self.assertEqual(sum(name in line for line in lines), 1)
        self.assertEqual(lines[-1], "All 5 judges ran.")

    def test_one_fail_gives_exit_1(self):
        code, text = self.run_main(good_set(rekordcrate=ev.FAIL))
        self.assertEqual(code, 1)
        self.assertIn(ev.FAIL, text)

    def test_dlp_not_installed_is_not_exit_0(self):
        code, _ = self.run_main(good_set(dlp_verify=ev.NOT_INSTALLED))
        self.assertNotEqual(code, 0)

    def test_not_run_judges_are_counted(self):
        judges = good_set(
            **{"crate-digger": ev.NOT_INSTALLED, "rekordbox": ev.SKIPPED}
        )
        code, text = self.run_main(judges)
        self.assertEqual(code, 0)
        self.assertEqual(
            text.splitlines()[-1],
            "2 of 5 judges did not run. This is not a full pass.",
        )

    def test_json_output(self):
        judges = [
            FakeJudge("dlp_verify", ev.PASS, "9 checks passed."),
            FakeJudge("anlz_verify", ev.PASS, "3 tracks are OK."),
            FakeJudge("rekordbox", ev.SKIPPED, "Manual step."),
        ]
        code, text = self.run_main(judges, "--json")
        data = json.loads(text)
        self.assertEqual(code, 0)
        self.assertEqual(
            [(j["name"], j["result"], j["detail"]) for j in data["judges"]],
            [
                ("dlp_verify", ev.PASS, "9 checks passed."),
                ("anlz_verify", ev.PASS, "3 tracks are OK."),
                ("rekordbox", ev.SKIPPED, "Manual step."),
            ],
        )
        self.assertEqual(data["not_run"], 1)

    def test_missing_stick_gives_exit_2(self):
        missing = os.path.join(self.stick, "does-not-exist")
        with contextlib.redirect_stderr(io.StringIO()):
            code = ev.main([missing], judges=good_set(), out=io.StringIO())
        self.assertEqual(code, 2)

    def test_usage_error_gives_exit_2(self):
        with contextlib.redirect_stderr(io.StringIO()):
            code = ev.main([], judges=good_set(), out=io.StringIO())
        self.assertEqual(code, 2)

    def test_exception_gives_fail_and_others_run(self):
        judges = good_set()
        judges[0] = FakeJudge("dlp_verify", None, error=ValueError("boom"))
        code, text = self.run_main(judges)
        self.assertEqual(code, 1)
        self.assertIn("ValueError: boom", text)
        self.assertTrue(all(j.called for j in judges))

    def test_reference_is_passed_on(self):
        seen = []

        class Spy(ev.Judge):
            name = "dlp_verify"

            def run(self, stick, reference):
                seen.append(reference)
                return ev.PASS, "ok"

        self.run_main([Spy()], "--reference", self.stick)
        self.assertEqual(seen, [self.stick])

    def test_help_commands(self):
        text = (
            "Usage: rekordcrate <COMMAND>\n\nCommands:\n  dump-pdb  x\n"
            "  dump-anlz  y\n\nOptions:\n  -h  help\n"
        )
        self.assertEqual(ev.help_commands(text), ["dump-pdb", "dump-anlz"])

    def test_missing_package_text(self):
        self.assertEqual(
            ev.missing_package("dlp_verify: No module named 'pyrekordbox'."),
            "pyrekordbox",
        )
        self.assertIsNone(ev.missing_package("FAIL integrity_check: x"))

    def test_crate_digger_not_installed_without_variable(self):
        old = os.environ.pop("CRATE_DIGGER_CMD", None)
        try:
            result, detail = ev.CrateDiggerJudge().run(self.stick, None)
        finally:
            if old is not None:
                os.environ["CRATE_DIGGER_CMD"] = old
        self.assertEqual(result, ev.NOT_INSTALLED)
        self.assertIn("CRATE_DIGGER_CMD", detail)

    def test_split_command_keeps_windows_backslashes(self):
        self.assertEqual(
            ev.split_command(r"java -jar C:\tools\cd.jar", windows=True),
            ["java", "-jar", r"C:\tools\cd.jar"],
        )
        self.assertEqual(
            ev.split_command(r'java -jar "C:\tools\cd.jar" -x', windows=True),
            ["java", "-jar", r"C:\tools\cd.jar", "-x"],
        )
        self.assertEqual(
            ev.split_command("java -jar 'a b.jar'", windows=False),
            ["java", "-jar", "a b.jar"],
        )

    def test_crate_digger_program_missing_is_not_installed(self):
        old = os.environ.get("CRATE_DIGGER_CMD")
        os.environ["CRATE_DIGGER_CMD"] = "no-such-program-xyz --flag"
        try:
            result, detail = ev.CrateDiggerJudge().run(self.stick, None)
        finally:
            if old is None:
                del os.environ["CRATE_DIGGER_CMD"]
            else:
                os.environ["CRATE_DIGGER_CMD"] = old
        self.assertEqual(result, ev.NOT_INSTALLED)
        self.assertIn("no-such-program-xyz", detail)

    def run_fake_script(self, body):
        path = os.path.join(self.stick, "fake_verify.py")
        with open(path, "w", newline="\n") as handle:
            handle.write(body)

        class Fake(ev.ScriptJudge):
            name = "fake"
            script = path

        return Fake().run(self.stick, None)

    def test_script_judge_exit_0_is_pass(self):
        result, _ = self.run_fake_script("print('PASS a')\n")
        self.assertEqual(result, ev.PASS)

    def test_script_judge_exit_1_is_fail(self):
        result, detail = self.run_fake_script(
            "import sys\nprint('FAIL x: bad')\nsys.exit(1)\n"
        )
        self.assertEqual(result, ev.FAIL)
        self.assertEqual(detail, "FAIL x: bad")

    def test_script_judge_import_error_is_not_installed(self):
        result, detail = self.run_fake_script("import no_such_pkg_xyz\n")
        self.assertEqual(result, ev.NOT_INSTALLED)
        self.assertIn("no_such_pkg_xyz", detail)

    def test_rekordbox_is_skipped(self):
        result, detail = ev.RekordboxJudge().run(self.stick, None)
        self.assertEqual(result, ev.SKIPPED)
        self.assertIn("Manual step", detail)


if __name__ == "__main__":
    unittest.main()
# End of AI-generated text.

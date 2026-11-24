#!/usr/bin/env python3
# AI-generated (Claude Code). Start of AI-generated text.
"""Run all export judges on a USB stick that Mix Two exported.

A stick counts as checked only when it passes Mix Two's own reader, the
independent parsers (crate-digger, rekordcrate) and rekordbox as the judge.
This script runs the judges that a script can run and prints one table:

    python tools/export_verify.py <stick> [--reference <stick>] [--json]

Judges, in this order:
  dlp_verify   tools/dlp_verify.py (needs sqlcipher3 and pyrekordbox).
  anlz_verify  tools/anlz_verify.py (needs pyrekordbox).
  rekordcrate  the rekordcrate program, when it is on the PATH. The script
               reads "rekordcrate --help" and uses a dump sub-command only
               when the help lists one that it knows.
  crate-digger a Java library with no command of its own. Set the environment
               variable CRATE_DIGGER_CMD to a command; the script runs it with
               the stick path as the last argument.
  rekordbox    always SKIPPED. Open the stick in rekordbox by hand.

Results: PASS, FAIL, NOT INSTALLED, SKIPPED. Exit code: 0 when no judge gives
FAIL and dlp_verify and anlz_verify give PASS; 1 otherwise; 2 for a usage error
or a stick path that does not exist. NOT INSTALLED and SKIPPED do not give
FAIL, but the last line of the output counts them, so nobody reads the result
as a full pass. With --json the script prints one JSON object and nothing else
on stdout. The script never writes to the stick.
"""

import argparse
import json
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys

PASS = "PASS"
FAIL = "FAIL"
NOT_INSTALLED = "NOT INSTALLED"
SKIPPED = "SKIPPED"

TOOLS_DIR = pathlib.Path(__file__).resolve().parent
REQUIRED_TO_PASS = ("dlp_verify", "anlz_verify")
# The rekordcrate sub-command that the script knows: it reads one analysis file.
REKORDCRATE_ANLZ_COMMAND = "dump-anlz"
NO_MODULE = re.compile(r"No module named '([^']+)'")
PROCESS_TIMEOUT_SECONDS = 1800


class Judge:
    """One judge. A subclass sets name and implements run."""

    name = ""

    def run(self, stick, reference):
        """Return (result, detail). A judge may raise: the caller turns an
        exception into FAIL."""
        raise NotImplementedError


def first_line(text, default=""):
    for line in text.splitlines():
        if line.strip():
            return line.strip()
    return default


def run_process(args):
    return subprocess.run(
        args,
        capture_output=True,
        text=True,
        errors="replace",
        timeout=PROCESS_TIMEOUT_SECONDS,
    )


def missing_package(output):
    """Name of the missing Python package in the output, or None."""
    match = NO_MODULE.search(output)
    return match.group(1).split(".")[0] if match else None


class ScriptJudge(Judge):
    """Runs one of the verify scripts of tools/ as a subprocess."""

    script = ""
    uses_reference = False

    def command(self, stick, reference):
        args = [sys.executable, "-I", str(TOOLS_DIR / self.script), str(stick)]
        if reference and self.uses_reference:
            args += ["--reference", str(reference)]
        return args

    def run(self, stick, reference):
        done = run_process(self.command(stick, reference))
        output = done.stdout + "\n" + done.stderr
        package = missing_package(output)
        if package:
            return NOT_INSTALLED, f"Python package {package} is not installed."
        lines = [line for line in done.stdout.splitlines() if line.strip()]
        if done.returncode == 0:
            return PASS, self.pass_detail(lines)
        failed = [line for line in lines if line.startswith("FAIL")]
        detail = failed[0] if failed else first_line(output, "No output.")
        return FAIL, detail

    def pass_detail(self, lines):
        return "Script exit code 0."


class DlpVerifyJudge(ScriptJudge):
    name = "dlp_verify"
    script = "dlp_verify.py"
    uses_reference = True

    def pass_detail(self, lines):
        count = sum(1 for line in lines if line.startswith("PASS"))
        return f"{count} checks passed."


class AnlzVerifyJudge(ScriptJudge):
    name = "anlz_verify"
    script = "anlz_verify.py"

    def pass_detail(self, lines):
        count = sum(1 for line in lines if line.startswith("OK"))
        return f"{count} tracks are OK."


def help_commands(help_text):
    """Names of the sub-commands in the help text of a program (the
    "Commands:" list of clap and similar)."""
    names = []
    in_list = False
    for line in help_text.splitlines():
        if re.match(r"^(commands|subcommands):", line.strip(), re.IGNORECASE):
            in_list = True
            continue
        if in_list:
            if not line.strip():
                if names:
                    break
                continue
            if not line.startswith((" ", "\t")):
                break
            names.append(line.split()[0])
    return names


class RekordcrateJudge(Judge):
    name = "rekordcrate"

    def run(self, stick, reference):
        program = shutil.which("rekordcrate")
        if not program:
            return NOT_INSTALLED, "rekordcrate is not on the PATH."
        helped = run_process([program, "--help"])
        commands = help_commands(helped.stdout + "\n" + helped.stderr)
        if REKORDCRATE_ANLZ_COMMAND not in commands:
            listed = ", ".join(commands) if commands else "none found"
            return SKIPPED, (
                "rekordcrate --help lists no known dump sub-command "
                f"(sub-commands: {listed})."
            )
        files = sorted(
            path
            for pattern in ("ANLZ0000.DAT", "ANLZ0000.EXT")
            for path in pathlib.Path(stick).rglob(pattern)
        )
        if not files:
            return SKIPPED, "The stick has no ANLZ files for rekordcrate."
        for path in files:
            done = run_process([program, REKORDCRATE_ANLZ_COMMAND, str(path)])
            if done.returncode != 0:
                reason = first_line(done.stderr + done.stdout, "No output.")
                return FAIL, f"{path.name} in {path.parent.name}: {reason}"
        return PASS, f"{len(files)} ANLZ files dumped."


class CrateDiggerJudge(Judge):
    name = "crate-digger"

    def run(self, stick, reference):
        command = os.environ.get("CRATE_DIGGER_CMD", "").strip()
        if not command:
            return NOT_INSTALLED, (
                "crate-digger is a Java library with no command. "
                "Set CRATE_DIGGER_CMD to a command that takes the stick path."
            )
        done = run_process(shlex.split(command) + [str(stick)])
        if done.returncode == 0:
            return PASS, "CRATE_DIGGER_CMD exit code 0."
        reason = first_line(done.stderr + done.stdout, "No output.")
        return FAIL, f"CRATE_DIGGER_CMD exit code {done.returncode}: {reason}"


class RekordboxJudge(Judge):
    name = "rekordbox"

    def run(self, stick, reference):
        return SKIPPED, (
            "Manual step: open the stick in rekordbox 6 and in rekordbox 7."
        )


def default_judges():
    return [
        DlpVerifyJudge(),
        AnlzVerifyJudge(),
        RekordcrateJudge(),
        CrateDiggerJudge(),
        RekordboxJudge(),
    ]


def run_judges(judges, stick, reference):
    """Run every judge. An exception in a judge gives FAIL; the others still
    run. Returns a list of dicts: name, result, detail."""
    results = []
    for judge in judges:
        try:
            result, detail = judge.run(stick, reference)
        except Exception as exc:  # a judge must never stop the others
            result = FAIL
            detail = f"{type(exc).__name__}: {exc}"
        results.append(
            {"name": judge.name, "result": result, "detail": detail}
        )
    return results


def exit_code(results):
    by_name = {item["name"]: item["result"] for item in results}
    if any(item["result"] == FAIL for item in results):
        return 1
    if any(by_name.get(name) != PASS for name in REQUIRED_TO_PASS):
        return 1
    return 0


def not_run_count(results):
    return sum(
        1 for item in results if item["result"] in (NOT_INSTALLED, SKIPPED)
    )


def summary_line(results):
    count = not_run_count(results)
    if count == 0:
        return f"All {len(results)} judges ran."
    return (
        f"{count} of {len(results)} judges did not run. "
        "This is not a full pass."
    )


def format_table(results):
    name_width = max([len("Judge")] + [len(item["name"]) for item in results])
    result_width = max(
        [len("Result")] + [len(item["result"]) for item in results]
    )
    lines = [f"{'Judge':<{name_width}}  {'Result':<{result_width}}  Detail"]
    for item in results:
        detail = " ".join(item["detail"].split())
        lines.append(
            f"{item['name']:<{name_width}}  "
            f"{item['result']:<{result_width}}  {detail}"
        )
    return lines


def main(argv=None, judges=None, out=None):
    out = out if out is not None else sys.stdout
    parser = argparse.ArgumentParser(
        description="Run all export judges on a USB stick."
    )
    parser.add_argument("stick", help="root folder of the exported stick")
    parser.add_argument(
        "--reference",
        metavar="STICK",
        help="root folder of a stick that rekordbox wrote",
    )
    parser.add_argument(
        "--json", action="store_true", help="print the results as JSON"
    )
    try:
        args = parser.parse_args(argv)
    except SystemExit as exc:
        return 2 if exc.code not in (0, None) else 0
    if not pathlib.Path(args.stick).is_dir():
        print(f"export_verify: {args.stick} is not a folder.", file=sys.stderr)
        return 2
    if args.reference and not pathlib.Path(args.reference).is_dir():
        print(
            f"export_verify: {args.reference} is not a folder.",
            file=sys.stderr,
        )
        return 2

    results = run_judges(
        judges if judges is not None else default_judges(),
        args.stick,
        args.reference,
    )
    code = exit_code(results)
    if args.json:
        json.dump(
            {
                "stick": args.stick,
                "judges": results,
                "not_run": not_run_count(results),
                "exit_code": code,
            },
            out,
            indent=2,
        )
        out.write("\n")
    else:
        for line in format_table(results) + [summary_line(results)]:
            print(line, file=out)
    return code


if __name__ == "__main__":
    sys.exit(main())
# End of AI-generated text.

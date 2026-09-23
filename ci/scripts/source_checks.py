"""Prepare the workspace and run source checks using native Stuart commands.

Shared by local developers and both provider adapters. Activate the intended
Python environment first: pip uses this interpreter and Stuart is found on PATH.
This entry point installs requirements and updates dependencies; for an inner
loop without preparation, invoke stuart_ci_build directly.
"""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CI_SETTINGS = ".pytool/CISettings.py"


def run(command: list[str]) -> None:
    """Log and execute an argument vector at the repo root without a shell.

    Output is inherited by the caller. Raises CalledProcessError on a nonzero
    exit or OSError when the executable cannot be started; neither is suppressed.
    """
    print(f"==> {shlex.join(command)}", flush=True)
    subprocess.run(command, cwd=REPO_ROOT, check=True)


def run_source_checks() -> None:
    """Install requirements, prepare CI dependencies, and run the selected checks.

    Uses CISettings.py, not platform build settings. Commands run sequentially;
    the first failure propagates and prevents all subsequent commands.
    """
    commands: tuple[list[str], ...] = (
        [sys.executable, "-m", "pip", "install", "-r", "pip-requirements.txt"],
        ["stuart_setup", "-c", CI_SETTINGS],
        ["stuart_update", "-c", CI_SETTINGS],
        [
            "stuart_ci_build",
            "-c",
            CI_SETTINGS,
            "--disable-all",
            "GuidCheck=run",
            "LineEndingCheck=run",
            "UncrustifyCheck=run",
        ],
    )
    for command in commands:
        run(command)


def main(argv: list[str] | None = None) -> None:
    """Parse the no-argument CLI (including --help), then run source checks.

    None reads sys.argv. Argparse exits for help or invalid arguments; execution
    failures propagate to the caller. This function does not create a venv.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args(argv)
    run_source_checks()


if __name__ == "__main__":
    main()

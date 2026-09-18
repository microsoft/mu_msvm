"""Prepare the workspace and run source checks using native Stuart commands."""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CI_SETTINGS = ".pytool/CISettings.py"


def run(command: list[str]) -> None:
    print(f"==> {shlex.join(command)}", flush=True)
    subprocess.run(command, cwd=REPO_ROOT, check=True)


def run_source_checks() -> None:
    commands = (
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args(argv)
    run_source_checks()


if __name__ == "__main__":
    main()

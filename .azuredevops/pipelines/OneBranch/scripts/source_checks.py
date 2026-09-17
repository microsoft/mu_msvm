"""Run MsvmPkg Stuart source checks, optionally provisioning a fresh CI checkout."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

from ci_common import run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--setup", action="store_true",
        help="Install Python dependencies and initialize top-level submodules before checking.",
    )
    args = parser.parse_args()
    os.chdir(Path(__file__).resolve().parents[4])

    try:
        if args.setup:
            run("Install Python dependencies", [
                sys.executable, "-m", "pip", "install", "-r", "pip-requirements.txt",
            ])
            # Prefer "stuart_setup -c .pytool/CISettings.py" so shared settings own setup.
            # Its recursive checkout reached libspdm's GitLab-hosted cmocka dependency,
            # which the OneBranch agent could not download (build 157900552).
            # These source checks use top-level repositories; restore Stuart setup
            # once fresh hosted agents can resolve all declared nested dependencies.
            run("Initialize source-check submodules", [
                "git", "-c", "core.autocrlf=false", "submodule", "update", "--init",
            ])
        run("Update source-check dependencies", ["stuart_update", "-c", ".pytool/CISettings.py"])
        run("Run source checks", [
            "stuart_ci_build", "-c", ".pytool/CISettings.py",
            "-p", "MsvmPkg", "-t", "NO-TARGET", "-a", "X64,AARCH64",
            "--disable-all", "GuidCheck=run", "LineEndingCheck=run", "UncrustifyCheck=run",
        ])
    except subprocess.CalledProcessError as exc:
        print(f"ERROR: Source checks failed: {' '.join(exc.cmd)} (exit {exc.returncode})", file=sys.stderr)
        sys.exit(exc.returncode)


if __name__ == "__main__":
    main()
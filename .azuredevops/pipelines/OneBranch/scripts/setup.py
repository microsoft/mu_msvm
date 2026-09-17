"""
Runs the pre-build setup phase: pip install, Stuart --setup, Stuart --update.

Each invocation fails-fast so a partial environment is never silently
passed to the build step.
"""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path

from ci_common import die, run

_DEFAULT_PIP_REQUIREMENTS = "pip-requirements.txt"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--platform-pkg",
        default=os.environ.get("PlatformPkg", ""),
        help="Platform package path (e.g. 'MsvmPkg'). Also read from $PlatformPkg env var.",
    )
    parser.add_argument(
        "--pip-requirements",
        default=_DEFAULT_PIP_REQUIREMENTS,
        help=f"pip requirements file (default: {_DEFAULT_PIP_REQUIREMENTS})",
    )
    args = parser.parse_args()

    platform_pkg = args.platform_pkg.strip()
    if not platform_pkg:
        die("--platform-pkg not specified and $PlatformPkg env var is not set.")

    pip_req = Path(args.pip_requirements)
    if not pip_req.exists():
        die(f"pip requirements file not found: {pip_req}")

    platform_build = Path(platform_pkg) / "PlatformBuild.py"
    if not platform_build.exists():
        die(f"PlatformBuild.py not found at: {platform_build}")

    try:
        run("python --version", ["python", "--version"])
        run(
            f"pip install -r {pip_req}",
            ["python", "-m", "pip", "install", "--upgrade", "-r", str(pip_req),
             "--force-reinstall", "--no-cache-dir"],
        )
        run(f"stuart_setup -c {platform_build}", ["stuart_setup", "-c", str(platform_build)])
        run(f"stuart_update -c {platform_build}", ["stuart_update", "-c", str(platform_build)])
    except subprocess.CalledProcessError as exc:
        die(f"Setup failed while running: {' '.join(exc.cmd)} (exit {exc.returncode})")


if __name__ == "__main__":
    main()

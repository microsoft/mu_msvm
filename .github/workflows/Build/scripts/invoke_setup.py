"""
Runs the pre-build setup phase: pip install + stuart_setup + stuart_update.

Each invocation fails-fast so a partial environment is never silently
passed to the build step.
"""

from __future__ import annotations

import argparse
import os

from ci_common import DEFAULT_PLATFORM_PKG, REPO_ROOT, VALID_TOOLCHAINS, die, run

_DEFAULT_PIP_REQUIREMENTS = "pip_requirement_stable.txt"


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--platform-pkg",
        default=os.environ.get("PlatformPkg", DEFAULT_PLATFORM_PKG),
        help=f"Platform package path (default: {DEFAULT_PLATFORM_PKG}).",
    )
    parser.add_argument(
        "--tools",
        choices=VALID_TOOLCHAINS,
        default=os.environ.get("TOOL_CHAIN_TAG", ""),
        help="Optional toolchain tag to pass through to Stuart.",
    )
    parser.add_argument(
        "--pip-requirements",
        default=_DEFAULT_PIP_REQUIREMENTS,
        help=f"pip requirements file relative to repo root (default: {_DEFAULT_PIP_REQUIREMENTS}).",
    )
    args = parser.parse_args()

    platform_pkg = args.platform_pkg.strip()
    if not platform_pkg:
        die("--platform-pkg not specified and $PlatformPkg env var is not set.")

    pip_req = REPO_ROOT / args.pip_requirements
    if not pip_req.exists():
        die(f"pip requirements file not found: {pip_req}")

    platform_build = REPO_ROOT / platform_pkg / "PlatformBuild.py"
    if not platform_build.exists():
        die(f"PlatformBuild.py not found at: {platform_build}")

    stuart_args = [f"TOOL_CHAIN_TAG={args.tools}"] if args.tools else []

    run("python --version", ["python", "--version"])
    run(
        f"pip install -r {pip_req}",
        ["python", "-m", "pip", "install", "--upgrade", "-r", str(pip_req)],
    )
    run("stuart_setup", ["stuart_setup", "-c", str(platform_build), *stuart_args], cwd=REPO_ROOT)
    run("stuart_update", ["stuart_update", "-c", str(platform_build), *stuart_args], cwd=REPO_ROOT)


if __name__ == "__main__":
    main()

"""Prepare and build one firmware flavor using native Stuart commands."""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
import sysconfig
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
PLATFORM_SETTINGS = "MsvmPkg/PlatformBuild.py"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", required=True, choices=("X64", "AARCH64"))
    parser.add_argument("--target", required=True, choices=("DEBUG", "RELEASE"))
    parser.add_argument("--tool-chain", required=True, choices=("VS2022", "CLANGPDB", "GCC"))
    parser.add_argument("--core", required=True, choices=("legacy", "patina"))
    parser.add_argument("--dry-run", action="store_true", help="Print commands without executing them")
    args = parser.parse_args(argv)

    flavor = [
        f"BUILD_ARCH={args.arch}",
        f"TARGET={args.target}",
        f"TOOL_CHAIN_TAG={args.tool_chain}",
        f"BLD_*_USE_LEGACY_C_CORE={'TRUE' if args.core == 'legacy' else 'FALSE'}",
    ]
    scripts_dir = Path(sysconfig.get_path("scripts"))
    suffix = ".exe" if os.name == "nt" else ""
    commands = [[sys.executable, "-m", "pip", "install", "-r", "pip-requirements.txt"]]
    for tool in ("stuart_setup", "stuart_update", "stuart_build"):
        commands.append([str(scripts_dir / (tool + suffix)), "-c", PLATFORM_SETTINGS, *flavor])
    commands[-1].extend([
        "BUILDREPORTING=TRUE",
        "BUILDREPORT_TYPES=PCD DEPEX FLASH BUILD_FLAGS LIBRARY",
        "LaunchLogOnSuccess=FALSE",
        "LaunchLogOnError=FALSE",
    ])

    for command in commands:
        print(f"==> {shlex.join(command)}", flush=True)
        if not args.dry_run:
            try:
                subprocess.run(command, cwd=REPO_ROOT, check=True)
            except subprocess.CalledProcessError as error:
                return error.returncode
    return 0


if __name__ == "__main__":
    sys.exit(main())

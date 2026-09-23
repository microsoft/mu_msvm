"""Prepare and build one firmware flavor using native Stuart commands.

The invoking Python environment supplies pip and Stuart executables. Setup,
update, and compilation run from the repository root in that order. Compiler
installation, matrix scheduling, credentials, and publishing belong to callers.
"""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
import sysconfig
from pathlib import Path
from typing import Literal

REPO_ROOT = Path(__file__).resolve().parents[2]
PLATFORM_SETTINGS = "MsvmPkg/PlatformBuild.py"


class BuildArguments(argparse.Namespace):
    """Typed CLI inputs populated and choice-validated by argparse.

    Host and compiler source describe execution, not the target architecture.
    An explicit Clang directory affects child processes only. Dry runs validate
    the requested flavor but do not require its host or compiler to be present.
    """

    arch: Literal["X64", "AARCH64"]
    target: Literal["DEBUG", "RELEASE"]
    tool_chain: Literal["VS2022", "CLANGPDB", "GCC"]
    core: Literal["legacy", "patina"]
    host: Literal["windows", "linux"]
    compiler_source: Literal["visual_studio", "windows_org", "distribution"] | None
    legacy_debugger: Literal["0", "1"]
    source_origin: Literal["unknown", "open", "closed"]
    clang_bin: Path | None
    dry_run: bool


def main(argv: list[str] | None = None) -> int:
    """Run one explicit flavor, stopping at the first failed command.

    Args:
        argv: CLI arguments, or None to read the process command line.

    Returns:
        Zero on success or dry run; otherwise the first subprocess exit code.

    Raises:
        SystemExit: Help (0) or an invalid/unsupported invocation (2).
        OSError: A subprocess cannot be started, for example missing Stuart.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", required=True, choices=("X64", "AARCH64"))
    parser.add_argument("--target", required=True, choices=("DEBUG", "RELEASE"))
    parser.add_argument("--tool-chain", required=True, choices=("VS2022", "CLANGPDB", "GCC"))
    parser.add_argument("--core", required=True, choices=("legacy", "patina"))
    parser.add_argument("--host", choices=("windows", "linux"), default="windows" if os.name == "nt" else "linux")
    parser.add_argument("--compiler-source", choices=("visual_studio", "windows_org", "distribution"))
    parser.add_argument("--legacy-debugger", choices=("0", "1"), default="0")
    parser.add_argument("--source-origin", choices=("unknown", "open", "closed"), default="unknown",
                        help="Source repository provenance for firmware metadata; never inferred from the host")
    parser.add_argument("--clang-bin", type=Path, help="Explicit compiler bin directory; required for Windows-org Clang")
    parser.add_argument("--dry-run", action="store_true", help="Print commands without executing them")
    args = parser.parse_args(argv, namespace=BuildArguments())

    compiler_source = args.compiler_source or ("visual_studio" if args.host == "windows" else "distribution")
    if args.arch == "AARCH64" and args.tool_chain == "VS2022":
        parser.error("ARM64 MSVC is not supported")
    if args.legacy_debugger == "1" and (
        args.source_origin != "closed" or args.arch != "X64" or args.core != "legacy"
        or args.tool_chain not in ("VS2022", "CLANGPDB")
    ):
        parser.error("Legacy debugger requires closed-source X64 VS2022 or CLANGPDB with the legacy core")
    if (args.host, args.tool_chain, compiler_source) not in {
        ("windows", "VS2022", "visual_studio"), ("windows", "CLANGPDB", "visual_studio"),
        ("windows", "CLANGPDB", "windows_org"), ("linux", "CLANGPDB", "distribution"),
        ("linux", "GCC", "distribution"),
    }:
        parser.error("Unsupported host/compiler combination")
    if compiler_source == "windows_org" and args.clang_bin is None:
        parser.error("Windows-org Clang requires --clang-bin; Visual Studio fallback is not allowed")
    if not args.dry_run:
        if args.host != ("windows" if os.name == "nt" else "linux"):
            parser.error(f"This flavor requires a {args.host} host")
        if args.clang_bin is not None and not (args.clang_bin / ("clang.exe" if args.host == "windows" else "clang")).is_file():
            parser.error("--clang-bin must contain the selected Clang executable")

    flavor: list[str] = [
        f"BUILD_ARCH={args.arch}",
        f"TARGET={args.target}",
        f"TOOL_CHAIN_TAG={args.tool_chain}",
        f"BLD_*_USE_LEGACY_C_CORE={'TRUE' if args.core == 'legacy' else 'FALSE'}",
    ]
    flavor.append(f"BLD_*_LEGACY_DEBUGGER={args.legacy_debugger}")
    environment: dict[str, str] = os.environ.copy()
    environment["SOURCE_ORIGIN"] = args.source_origin
    if args.clang_bin is not None:
        environment["CLANG_BIN"] = f"{args.clang_bin.resolve().as_posix()}/"
        print(f"CLANG_BIN={environment['CLANG_BIN']}", flush=True)
    scripts_dir = Path(sysconfig.get_path("scripts"))
    suffix = ".exe" if os.name == "nt" else ""
    commands: list[list[str]] = [[sys.executable, "-m", "pip", "install", "-r", "pip-requirements.txt"]]
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
                subprocess.run(command, cwd=REPO_ROOT, check=True, env=environment)
            except subprocess.CalledProcessError as error:
                return error.returncode
    return 0


if __name__ == "__main__":
    sys.exit(main())

"""
Runs the main Hyper-V UEFI build for one matrix variant.

Invokes PlatformBuild.py with the canonical Stuart args. Always dumps
Build/ listing and Build/B*.txt log files in a finally block so a failed
job still produces actionable output in the pipeline log.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

from ci_common import VALID_ARCHS, VALID_TARGETS, VALID_TOOLCHAINS, Arch, Target, ToolChainTag, die

# Fixed Stuart args appended to every build invocation.
_FIXED_BUILD_ARGS: list[str] = [
    "BUILDREPORTING=TRUE",
    "BUILDREPORT_TYPES=PCD DEPEX FLASH BUILD_FLAGS LIBRARY",
    "LaunchLogOnSuccess=FALSE",
    "LaunchLogOnError=FALSE",
]


def _dump_build_logs() -> None:
    build_dir = Path("Build")
    print("== Build/ listing ==")
    if build_dir.exists():
        for entry in sorted(build_dir.iterdir()):
            size = entry.stat().st_size if entry.is_file() else 0
            print(f"  {entry.name:<45}  {size:>10} bytes")
    else:
        print("  (Build directory missing)")

    print("== Build/B*.txt contents ==")
    if build_dir.exists():
        for txt in sorted(build_dir.glob("B*.txt")):
            print(f"---- {txt.name} ----")
            print(txt.read_text(errors="replace"))


def run_build(
    arch: Arch,
    target: Target,
    tool_chain_tag: ToolChainTag,
    extra: str,
    platform_pkg: str,
) -> None:
    platform_build = Path(platform_pkg) / "PlatformBuild.py"
    if not platform_build.exists():
        die(f"PlatformBuild.py not found at: {platform_build}")

    extra_tokens = extra.split() if extra.strip() else []
    cmd = [
        "stuart_build", "-c", str(platform_build),
        *extra_tokens,
        f"TOOL_CHAIN_TAG={tool_chain_tag}",
        f"TARGET={target}",
        f"BUILD_ARCH={arch}",
        *_FIXED_BUILD_ARGS,
    ]
    print(f"== Command: {' '.join(cmd)} ==")

    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
    finally:
        _dump_build_logs()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--arch", required=True, choices=VALID_ARCHS)
    parser.add_argument("--target", required=True, choices=VALID_TARGETS)
    parser.add_argument("--tool-chain-tag", required=True, choices=VALID_TOOLCHAINS)
    parser.add_argument("--extra", default="", help="Extra PlatformBuild.py args (whitespace-separated)")
    parser.add_argument(
        "--platform-pkg",
        default=os.environ.get("PlatformPkg", "MsvmPkg"),
        help="Platform package path. Also read from $PlatformPkg env var.",
    )
    args = parser.parse_args()

    platform_pkg = args.platform_pkg.strip()
    if not platform_pkg:
        die("--platform-pkg not specified and $PlatformPkg env var is not set.")

    run_build(
        arch=args.arch,
        target=args.target,
        tool_chain_tag=args.tool_chain_tag,
        extra=args.extra,
        platform_pkg=platform_pkg,
    )


if __name__ == "__main__":
    main()

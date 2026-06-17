"""
Runs the main mu_msvm UEFI build for one variant.

Invokes stuart_build with canonical args. Sets CLANG_BIN for CLANGPDB
variants (pointing at the Clang shipped with Visual Studio 2022).
Always dumps Build/ listing and Build/B*.txt log files in a finally block
so a failed CI job still produces actionable output.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

from ci_common import (
    DEFAULT_PLATFORM_PKG,
    REPO_ROOT,
    VALID_ARCHS,
    VALID_TARGETS,
    VALID_TOOLCHAINS,
    Arch,
    Target,
    ToolChainTag,
    die,
    run,
)

# Fixed Stuart args appended to every build invocation.
_FIXED_BUILD_ARGS: list[str] = [
    "BUILDREPORTING=TRUE",
    "BUILDREPORT_TYPES=PCD DEPEX FLASH BUILD_FLAGS LIBRARY",
    "LaunchLogOnSuccess=FALSE",
    "LaunchLogOnError=FALSE",
]


def _dump_build_logs() -> None:
    build_dir = REPO_ROOT / "Build"
    print("== Build/ listing ==", flush=True)
    if build_dir.exists():
        for entry in sorted(build_dir.iterdir()):
            size = entry.stat().st_size if entry.is_file() else 0
            print(f"  {entry.name:<45}  {size:>10} bytes")
    else:
        print("  (Build directory missing)")

    print("== Build/B*.txt contents ==", flush=True)
    if build_dir.exists():
        for txt in sorted(build_dir.glob("B*.txt")):
            print(f"---- {txt.name} ----")
            print(txt.read_text(errors="replace"))


def _resolve_clang_bin() -> str:
    """Locate the Clang shipped with Visual Studio 2022 on a GitHub Windows runner.

    Returns a path with a trailing slash (required by EDK2).
    """
    if sys.platform != "win32":
        die("CLANGPDB build is currently only supported on Windows runners.")

    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    if not vswhere.exists():
        die(f"vswhere.exe not found: {vswhere}")

    result = subprocess.run(
        [
            str(vswhere),
            "-latest",
            "-products", "*",
            "-version", "[17.0,18.0)",
            "-property", "installationPath",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    vs_path = Path(result.stdout.strip())
    if not vs_path.exists():
        die(f"Visual Studio 2022 installation path not found: {vs_path}")

    clang_bin = vs_path / "VC" / "Tools" / "Llvm" / "x64" / "bin"
    if not (clang_bin / "clang.exe").exists():
        die(f"clang.exe not found at: {clang_bin / 'clang.exe'}")

    # EDK2 requires trailing slash.
    return str(clang_bin) + os.sep


def run_build(
    arch: Arch,
    target: Target,
    tools: ToolChainTag,
    extra: str,
    platform_pkg: str,
) -> None:
    platform_build = REPO_ROOT / platform_pkg / "PlatformBuild.py"
    if not platform_build.exists():
        die(f"PlatformBuild.py not found at: {platform_build}")

    env = os.environ.copy()
    if tools == "CLANGPDB" and "CLANG_BIN" not in env:
        env["CLANG_BIN"] = _resolve_clang_bin()
        print(f"== CLANG_BIN = {env['CLANG_BIN']} ==", flush=True)

    extra_tokens = extra.split() if extra.strip() else []
    cmd = [
        "stuart_build", "-c", str(platform_build), "--verbose",
        *extra_tokens,
        f"TOOL_CHAIN_TAG={tools}",
        f"TARGET={target}",
        f"BUILD_ARCH={arch}",
        *_FIXED_BUILD_ARGS,
    ]
    try:
        run("stuart_build", cmd, env=env, cwd=REPO_ROOT)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
    finally:
        _dump_build_logs()


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--arch", required=True, choices=VALID_ARCHS)
    parser.add_argument("--target", required=True, choices=VALID_TARGETS)
    parser.add_argument("--tools", required=True, choices=VALID_TOOLCHAINS,
                        help="Toolchain tag (matches stuart's TOOL_CHAIN_TAG).")
    parser.add_argument("--extra", default="",
                        help="Extra Stuart build args (whitespace-separated).")
    parser.add_argument(
        "--platform-pkg",
        default=os.environ.get("PlatformPkg", DEFAULT_PLATFORM_PKG),
        help=f"Platform package path (default: {DEFAULT_PLATFORM_PKG}).",
    )
    args = parser.parse_args()

    run_build(
        arch=args.arch,
        target=args.target,
        tools=args.tools,
        extra=args.extra,
        platform_pkg=args.platform_pkg.strip(),
    )


if __name__ == "__main__":
    main()

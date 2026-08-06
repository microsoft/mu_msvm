"""
Run one or all CI build variants locally, invoking the same scripts CI uses.

Skipped vs. the GitHub Actions workflow:
    install_vs_components       (assumes developer machine already has VS components)
    upload-artifact             (no artifact upload locally)

Usage examples:
    # List variants from data/matrix.yml.
    python .github/workflows/Build/scripts/invoke_ci_locally.py --list

    # Run a single variant.
    python .github/workflows/Build/scripts/invoke_ci_locally.py \\
        --arch X64 --target DEBUG --tools VS2022

    # Run every variant in matrix.yml sequentially (fail-fast).
    python .github/workflows/Build/scripts/invoke_ci_locally.py --all

    # Skip artifact staging for a faster inner-loop build.
    python .github/workflows/Build/scripts/invoke_ci_locally.py \\
        --arch X64 --target DEBUG --tools VS2022 --skip-stage
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
    SCRIPTS_DIR,
    VALID_ARCHS,
    VALID_TARGETS,
    VALID_TOOLCHAINS,
    die,
    load_variants,
)


def _run_script(script: str, *args: str) -> None:
    cmd = [sys.executable, str(SCRIPTS_DIR / script), *args]
    print(f"\n>>> {' '.join(cmd)}\n", flush=True)
    subprocess.run(cmd, check=True)


def _run_variant(arch: str, target: str, tools: str, platform_pkg: str,
                 out_dir: Path, skip_stage: bool, skip_setup: bool) -> None:
    if not skip_setup:
        _run_script("invoke_setup.py", "--platform-pkg", platform_pkg, "--tools", tools)

    _run_script(
        "invoke_build.py",
        "--arch", arch,
        "--target", target,
        "--tools", tools,
        "--platform-pkg", platform_pkg,
    )

    if not skip_stage:
        variant_out = out_dir / f"{target}_{arch}_{tools}"
        _run_script(
            "stage_artifacts.py",
            "--arch", arch,
            "--target", target,
            "--tools", tools,
            "--out-dir", str(variant_out),
        )

    print(f"\n==== Done: {target} {arch} {tools} ====")


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--list", action="store_true", help="List variants and exit.")
    parser.add_argument("--all", action="store_true",
                        help="Run every variant in data/matrix.yml sequentially.")
    parser.add_argument("--arch", choices=VALID_ARCHS)
    parser.add_argument("--target", choices=VALID_TARGETS)
    parser.add_argument("--tools", choices=VALID_TOOLCHAINS)
    parser.add_argument("--platform-pkg", default=DEFAULT_PLATFORM_PKG)
    parser.add_argument("--out-dir", type=Path, default=REPO_ROOT / "LocalCiOutput",
                        help="Artifact staging root (default: <repo>/LocalCiOutput/).")
    parser.add_argument("--skip-stage", action="store_true",
                        help="Skip stage_artifacts step for a faster inner-loop build.")
    parser.add_argument("--skip-setup", action="store_true",
                        help="Skip stuart_setup/update (assumes already done).")
    args = parser.parse_args()

    variants = load_variants()

    if args.list:
        print("variants from data/matrix.yml:")
        for v in variants:
            print(f"  arch={v['arch']:<8}  target={v['target']:<8}  tools={v['tools']}")
        return

    # Ensure scripts find each other regardless of caller cwd.
    os.chdir(REPO_ROOT)

    if args.all:
        if any([args.arch, args.target, args.tools]):
            die("--all is mutually exclusive with --arch/--target/--tools.")
        for v in variants:
            _run_variant(v["arch"], v["target"], v["tools"],
                         args.platform_pkg, args.out_dir,
                         args.skip_stage, args.skip_setup)
        return

    if not (args.arch and args.target and args.tools):
        die("Must specify --arch + --target + --tools, or use --all / --list.")

    _run_variant(args.arch, args.target, args.tools,
                 args.platform_pkg, args.out_dir,
                 args.skip_stage, args.skip_setup)


if __name__ == "__main__":
    main()

"""
Run one CI build variant locally, invoking the same scripts the pipeline uses.

Mirrors jobs.yml for a single matrix row without an ADO runner. Run from
the repo root after installing toolchain and ensuring Python 3.10+ is active.

Skipped vs. the pipeline:
    set_repo_auth.py    (assumes developer git credentials are configured)
    NuGet download      (assumes toolchain already installed)
    publish_symbols     (no symbol server upload locally)
    pack_nuget          (no .nupkg needed locally)
    OneBranch vpack / artifact upload

Usage:
    python .azuredevops/pipelines/OneBranch/scripts/run_local_ci.py \\
        --arch X64 --target DEBUG --tool-chain-tag VS2022
"""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path

from ci_common import VALID_ARCHS, VALID_TARGETS, VALID_TOOLCHAINS

_SCRIPTS_DIR = Path(__file__).parent


def _run_script(script: str, *args: str) -> None:
    """Run a sibling script in the same Python interpreter."""
    cmd = ["python", str(_SCRIPTS_DIR / script), *args]
    subprocess.run(cmd, check=True)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--arch", required=True, choices=VALID_ARCHS)
    parser.add_argument("--target", required=True, choices=VALID_TARGETS)
    parser.add_argument("--tool-chain-tag", required=True, choices=VALID_TOOLCHAINS)
    parser.add_argument("--extra", default="", help="Extra PlatformBuild.py args")
    parser.add_argument("--platform-pkg", default="MsvmPkg")
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path.cwd() / "LocalCiOutput",
        help="Artifact output directory (default: LocalCiOutput/)",
    )
    parser.add_argument(
        "--skip-stage",
        action="store_true",
        help="Skip artifact staging for a faster inner-loop build",
    )
    args = parser.parse_args()

    os.environ["PlatformPkg"] = args.platform_pkg

    print("==== setup ====")
    _run_script("setup.py", "--platform-pkg", args.platform_pkg)

    print("==== build ====")
    build_args = [
        "--arch", args.arch,
        "--target", args.target,
        "--tool-chain-tag", args.tool_chain_tag,
        "--platform-pkg", args.platform_pkg,
    ]
    if args.extra:
        build_args += ["--extra", args.extra]
    _run_script("build.py", *build_args)

    if not args.skip_stage:
        build_dir = f"Build/Msvm{args.arch}/{args.target}_{args.tool_chain_tag}"
        print(f"==== stage_artifacts (build_dir={build_dir}, out_dir={args.out_dir}) ====")
        _run_script(
            "stage_artifacts.py",
            "--arch", args.arch,
            "--target", args.target,
            "--tool-chain-tag", args.tool_chain_tag,
            "--build-dir", build_dir,
            "--out-dir", str(args.out_dir),
            "--job-name", "local",
        )

    extra_label = f" ({args.extra})" if args.extra else ""
    print(f"\n==== Done: {args.target} {args.arch} {args.tool_chain_tag}{extra_label} ====")
    if not args.skip_stage:
        print(f"Output: {args.out_dir}")


if __name__ == "__main__":
    main()

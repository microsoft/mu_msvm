"""
Stages build outputs for upload as a CI artifact.

Lays out:
    <out_dir>/MSVM.fd
    <out_dir>/MAP/
    <out_dir>/PDB/
    <out_dir>/Build Logs/  (flattened)

The matching upload-artifact step just points at <out_dir>.
"""

from __future__ import annotations

import argparse
import fnmatch
import shutil
from pathlib import Path

from ci_common import (
    REPO_ROOT,
    VALID_ARCHS,
    VALID_TARGETS,
    VALID_TOOLCHAINS,
    Arch,
    Target,
    ToolChainTag,
    build_dir_for,
)

# Log files to collect from anywhere under Build/ (case-insensitive fnmatch).
_LOG_PATTERNS: tuple[str, ...] = (
    "BuildLog.txt",
    "BUILDLOG_*.txt",
    "BUILDLOG_*.md",
    "BUILD_REPORT.txt",
    "CI_*.txt",
    "CI_*.md",
    "CISETUP.txt",
    "SETUPLOG.txt",
    "UPDATE_LOG.txt",
    "PREVALLOG.txt",
    "BUILD_TOOLS_REPORT.html",
    "OVERRIDELOG.TXT",
    "BASETOOLS_BUILD*.*",
    "FD_REPORT.HTML",
)


def _is_log_file(name: str) -> bool:
    n = name.lower()
    return any(fnmatch.fnmatch(n, pat.lower()) for pat in _LOG_PATTERNS)


def _copy_tree(src: Path, dst: Path, label: str) -> None:
    if not src.exists():
        print(f"WARNING: {label} source missing (skipping): {src}")
        return
    shutil.copytree(src, dst, dirs_exist_ok=True)
    print(f"OK: copied {label}: {src} -> {dst}")


def _copy_file(src: Path, dst_dir: Path, label: str) -> None:
    if not src.exists():
        print(f"WARNING: {label} source missing (skipping): {src}")
        return
    dst_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst_dir / src.name)
    print(f"OK: copied {label}: {src} -> {dst_dir / src.name}")


def stage(arch: Arch, target: Target, tools: ToolChainTag, out_dir: Path) -> None:
    build_dir = REPO_ROOT / build_dir_for(arch, target, tools)
    out_dir.mkdir(parents=True, exist_ok=True)

    _copy_file(build_dir / "FV" / "MSVM.fd", out_dir, "MSVM.fd")
    _copy_tree(build_dir / "MAP", out_dir / "MAP", "MAP/")
    _copy_tree(build_dir / "PDB", out_dir / "PDB", "PDB/")

    log_dest = out_dir / "Build Logs"
    log_dest.mkdir(parents=True, exist_ok=True)
    if build_dir.exists():
        for f in build_dir.rglob("*"):
            if f.is_file() and _is_log_file(f.name):
                try:
                    shutil.copy2(f, log_dest / f.name)
                except OSError as exc:
                    print(f"WARNING: failed to stage log {f}: {exc}")

    print(f"\nStaging complete: {out_dir}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--arch", required=True, choices=VALID_ARCHS)
    parser.add_argument("--target", required=True, choices=VALID_TARGETS)
    parser.add_argument("--tools", required=True, choices=VALID_TOOLCHAINS)
    parser.add_argument("--out-dir", required=True, type=Path,
                        help="Staging directory the upload-artifact step will collect.")
    args = parser.parse_args()

    stage(args.arch, args.target, args.tools, args.out_dir.resolve())


if __name__ == "__main__":
    main()

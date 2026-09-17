"""
Stages build outputs into the OneBranch ob_outputDirectory.

Produces:
    <out_dir>/Build Logs <job_name>/                 -- flattened log set
    <out_dir>/Firmware Binary File <target>_<arch>/  -- MSVM.fd
    <out_dir>/Firmware PDB Files   <target>_<arch>/  -- *.pdb (non-GCC only)
    <out_dir>/Firmware Map Files   <target>_<arch>/  -- *.map
    <out_dir>/<build_dir>/Bin/                       -- vpack staging dir:
        MSVM.fd, Dsdt.aml, BiosInterface.h, PDB/ (non-GCC only)

PDB and symbol files are skipped for GCC builds (no CodeView output).
"""

from __future__ import annotations

import argparse
import fnmatch
import os
import shutil
from pathlib import Path

from ci_common import VALID_ARCHS, VALID_TARGETS, VALID_TOOLCHAINS, Arch, Target, ToolChainTag

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
    "TestSuites.xml",
    "BUILD_TOOLS_REPORT.html",
    "OVERRIDELOG.TXT",
    "BASETOOLS_BUILD*.*",
    "FD_REPORT.HTML",
)


def _is_log_file(name: str) -> bool:
    return any(fnmatch.fnmatch(name.lower(), pat.lower()) for pat in _LOG_PATTERNS)


def _copy_file(src: Path, dst_dir: Path) -> None:
    """Copy *src* file into *dst_dir*, warning if absent."""
    if not src.exists():
        print(f"WARNING: source missing (skipping): {src}")
        return
    shutil.copy2(src, dst_dir)


def _copy_tree(src: Path, dst_dir: Path) -> None:
    """Copy directory *src* as a sub-tree of *dst_dir*, warning if absent."""
    if not src.exists():
        print(f"WARNING: source missing (skipping): {src}")
        return
    shutil.copytree(src, dst_dir / src.name, dirs_exist_ok=True)


def stage_artifacts(
    arch: Arch,
    target: Target,
    tool_chain_tag: ToolChainTag,
    build_dir: Path,
    out_dir: Path,
    job_name: str,
) -> None:
    is_gcc = tool_chain_tag == "GCC"
    arch_target = f"{target}_{arch}"

    # -- Build logs (flattened) --
    log_dest = out_dir / f"Build Logs {job_name}"
    log_dest.mkdir(parents=True, exist_ok=True)
    if build_dir.exists():
        # Logs are intentionally flattened by filename into one folder for ADO
        # artifact browsing simplicity. If two files share a name, last copy wins.
        for f in build_dir.rglob("*"):
            if f.is_file() and _is_log_file(f.name):
                try:
                    shutil.copy2(f, log_dest / f.name)
                except OSError as exc:
                    print(f"WARNING: failed to stage log {f}: {exc}")

    # -- Firmware binary --
    fd_dest = out_dir / f"Firmware Binary File {arch_target}"
    fd_dest.mkdir(parents=True, exist_ok=True)
    _copy_file(build_dir / "FV" / "MSVM.fd", fd_dest)

    # -- PDB files (non-GCC only) --
    if not is_gcc:
        pdb_dest = out_dir / f"Firmware PDB Files {arch_target}"
        pdb_dest.mkdir(parents=True, exist_ok=True)
        pdb_src = build_dir / "PDB"
        if pdb_src.exists():
            shutil.copytree(pdb_src, pdb_dest, dirs_exist_ok=True)
        else:
            print(f"WARNING: PDB source missing (skipping): {pdb_src}")

    # -- MAP files --
    map_dest = out_dir / f"Firmware Map Files {arch_target}"
    map_dest.mkdir(parents=True, exist_ok=True)
    map_src = build_dir / "MAP"
    if map_src.exists():
        shutil.copytree(map_src, map_dest, dirs_exist_ok=True)
    else:
        print(f"WARNING: MAP source missing (skipping): {map_src}")

    # -- Vpack staging: ob_createvpack_vpackdirectory = <out_dir>/<build_dir>/Bin --
    vpack_dir = out_dir / build_dir / "Bin"
    vpack_dir.mkdir(parents=True, exist_ok=True)

    _copy_file(build_dir / "FV" / "MSVM.fd", vpack_dir)
    _copy_file(build_dir / arch / "MsvmPkg" / "AcpiTables" / "AcpiTables" / "OUTPUT" / "Dsdt.aml", vpack_dir)
    _copy_file(Path("MsvmPkg") / "Include" / "BiosInterface.h", vpack_dir)

    if not is_gcc:
        _copy_tree(build_dir / "PDB", vpack_dir)

    print("Artifact staging complete.")
    print(f"  Logs:     {log_dest}")
    print(f"  Firmware: {fd_dest}")
    print(f"  Maps:     {map_dest}")
    print(f"  Vpack:    {vpack_dir}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--arch", required=True, choices=VALID_ARCHS)
    parser.add_argument("--target", required=True, choices=VALID_TARGETS)
    parser.add_argument("--tool-chain-tag", required=True, choices=VALID_TOOLCHAINS)
    parser.add_argument("--build-dir", required=True, type=Path, help="Workspace-relative build output dir")
    parser.add_argument("--out-dir", required=True, type=Path, help="Absolute path to ob_outputDirectory")
    parser.add_argument(
        "--job-name",
        default=os.environ.get("AGENT_JOBNAME", "local"),
        help="ADO job name for artifact folder. Falls back to 'local'.",
    )
    args = parser.parse_args()

    stage_artifacts(
        arch=args.arch,
        target=args.target,
        tool_chain_tag=args.tool_chain_tag,
        build_dir=args.build_dir,
        out_dir=args.out_dir,
        job_name=args.job_name or "local",
    )


if __name__ == "__main__":
    main()

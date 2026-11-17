"""Read firmware version metadata and validate staged artifact provenance."""

from __future__ import annotations

import argparse
import json
import re
import tomllib
from pathlib import Path

from ci.scripts.matrix import SelectedBuild
from ci.scripts.stage_artifacts import select_flavor


def base_version() -> str:
    with Path("MsvmPkg/FirmwareVersion.toml").open("rb") as stream:
        version = str(tomllib.load(stream)["release"]["version"])
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
        raise ValueError("Release version must be major.minor")
    return version


def validate_firmware(
    firmware: Path, flavor: SelectedBuild, commit: str, origin: str, official: bool,
    allow_dirty: bool = False,
) -> None:
    if official and allow_dirty:
        raise ValueError("Official firmware cannot allow dirty-worktree provenance")
    if not re.fullmatch(r"[0-9a-fA-F]{40}", commit):
        raise ValueError("Expected a full Git commit SHA")
    info = json.loads((firmware / "BuildInfo.json").read_text(encoding="utf-8"))
    if info != flavor:
        raise ValueError(f"Artifact flavor does not match the selected matrix row: {firmware}")
    metadata = json.loads((firmware / "FwVersion/FirmwareVersion.json").read_text(encoding="utf-8"))
    mismatches = []
    for field, expected in (("git_commit", commit), ("source_origin", origin), ("base_version", base_version())):
        if metadata.get(field) != expected:
            mismatches.append(f"{field}: expected {expected!r}, got {metadata.get(field)!r}")
    flags = metadata.get("flags")
    if type(flags) is not int:
        mismatches.append(f"flags must be an integer, got {flags!r}")
    else:
        if bool(flags & 2) != official:
            mismatches.append(f"official flag: expected {official}, got {bool(flags & 2)}")
        if flags & 1:
            message = "dirty-worktree flag is set; inspect the firmware build's Git status diagnostics"
            if allow_dirty:
                print(f"WARNING: test artifact: {message}; provenance remains unchanged", flush=True)
            else:
                mismatches.append(message)
    if mismatches:
        raise ValueError(f"Artifact provenance/version does not match this build: {firmware}: {'; '.join(mismatches)}")
    required = [firmware / "FV/MSVM.fd", firmware / "FwVersion/FwVersionBlob.bin"]
    for file in required:
        if not file.is_file() or file.stat().st_size == 0:
            raise ValueError(f"Required artifact is missing or empty: {file}")
    if not list((firmware / "MAP").glob("*.map")):
        raise ValueError(f"Missing MAP artifacts: {firmware}")
    if flavor["tool_chain"] != "GCC" and not list((firmware / "PDB").glob("*.pdb")):
        raise ValueError(f"Missing PDB artifacts: {firmware}")


class Arguments(argparse.Namespace):
    command: str
    matrix: Path
    source: Path
    commit: str
    build_id: str
    official: bool
    origin: str
    allow_dirty: bool


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("version")
    child = subparsers.add_parser("validate")
    child.add_argument("--matrix", required=True, type=Path)
    child.add_argument("--source", required=True, type=Path)
    child.add_argument("--commit", required=True)
    child.add_argument("--build-id", required=True)
    child.add_argument("--origin", required=True, choices=("open", "closed", "unknown"))
    child.add_argument("--official", action="store_true")
    child.add_argument("--allow-dirty", action="store_true")
    args = parser.parse_args(argv, namespace=Arguments())
    try:
        if args.command == "version":
            print(base_version())
        else:
            validate_firmware(args.source, select_flavor(args.matrix, args.build_id),
                              args.commit, args.origin, args.official, args.allow_dirty)
    except (OSError, ValueError, KeyError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()

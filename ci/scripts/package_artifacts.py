"""Validate staged firmware and prepare archives or a NuGet manifest, without publishing."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import tarfile
import tomllib
import xml.etree.ElementTree as ET
from pathlib import Path

from ci.scripts.matrix import SelectedBuild, label_builds, select_builds
from ci.scripts.stage_artifacts import ado_layout, select_flavor


def base_version() -> str:
    with Path("MsvmPkg/FirmwareVersion.toml").open("rb") as stream:
        version = str(tomllib.load(stream)["release"]["version"])
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
        raise ValueError("Release version must be major.minor")
    return version


def validate_firmware(firmware: Path, flavor: SelectedBuild, commit: str, origin: str, official: bool) -> None:
    if not re.fullmatch(r"[0-9a-fA-F]{40}", commit):
        raise ValueError("Expected a full Git commit SHA")
    info = json.loads((firmware / "BuildInfo.json").read_text(encoding="utf-8"))
    if info != flavor:
        raise ValueError(f"Artifact flavor does not match the selected matrix row: {firmware}")
    metadata = json.loads((firmware / "FwVersion/FirmwareVersion.json").read_text(encoding="utf-8"))
    if (metadata.get("git_commit") != commit or metadata.get("source_origin") != origin
            or metadata.get("base_version") != base_version()
            or type(metadata.get("flags")) is not int
            or bool(metadata["flags"] & 2) != official or metadata["flags"] & 1):
        raise ValueError(f"Artifact provenance/version does not match this build: {firmware}")
    required = [firmware / "FV/MSVM.fd", firmware / "FwVersion/FwVersionBlob.bin"]
    for file in required:
        if not file.is_file() or file.stat().st_size == 0:
            raise ValueError(f"Required artifact is missing or empty: {file}")
    if not list((firmware / "MAP").glob("*.map")):
        raise ValueError(f"Missing MAP artifacts: {firmware}")
    if flavor["tool_chain"] != "GCC" and not list((firmware / "PDB").glob("*.pdb")):
        raise ValueError(f"Missing PDB artifacts: {firmware}")


def release_archives(matrix: Path, source: Path, output: Path, commit: str) -> None:
    rows = select_builds(matrix)
    labeled = label_builds(rows, "github")
    expected = {row["artifact_name"] for row in labeled}
    if {path.name for path in source.iterdir()} != expected:
        raise ValueError("Downloaded firmware artifacts do not exactly match the release matrix")
    for row, label in zip(rows, labeled):
        validate_firmware(source / label["artifact_name"], row, commit, "open", True)
    output.mkdir(parents=True, exist_ok=False)
    for row in labeled:
        name = row["artifact_name"]
        with tarfile.open(output / f"{name}.tar.gz", "w:gz") as archive:
            for child in sorted((source / name).iterdir()):
                archive.add(child, arcname=child.name)


def nuget_manifest(
    matrix: Path, identifier: str, source: Path, output: Path,
    commit: str, repository: str, branch: str, official: bool,
    staging_output: Path | None = None,
) -> None:
    flavor = select_flavor(matrix, identifier)
    if flavor.get("shipping") != "true" or flavor["host"] != "windows":
        raise ValueError("Only shipping Windows flavors may be packaged")
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", repository):
        raise ValueError("Invalid package repository name")
    if not re.fullmatch(r"[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*", branch):
        raise ValueError("Branch name is not a valid NuGet prerelease identifier")
    firmware = source / "firmware"
    validate_firmware(firmware, flavor, commit, "closed", official)
    package = source / "package/Bin"
    for name in ("MSVM.fd", "Dsdt.aml", "BiosInterface.h", "FirmwareVersion.json", "FwVersionBlob.bin", "BuildInfo.json"):
        if not (package / name).is_file() or (package / name).stat().st_size == 0:
            raise ValueError(f"Missing package payload: {package / name}")
    for source_file, package_file in (
        (firmware / "FV/MSVM.fd", package / "MSVM.fd"),
        (firmware / "FwVersion/FirmwareVersion.json", package / "FirmwareVersion.json"),
        (firmware / "FwVersion/FwVersionBlob.bin", package / "FwVersionBlob.bin"),
        (firmware / "BuildInfo.json", package / "BuildInfo.json"),
    ):
        if source_file.read_bytes() != package_file.read_bytes():
            raise ValueError(f"Package payload differs from firmware artifact: {package_file}")
    if staging_output is not None:
        if staging_output.resolve().is_relative_to(source.resolve()) or source.resolve().is_relative_to(staging_output.resolve()):
            raise ValueError("Publication staging must not overlap downloaded artifacts")
        for directory in ("firmware", "logs", "package"):
            shutil.copytree(source / directory, staging_output / directory)
        ado_layout(staging_output, flavor)
        firmware = staging_output / "firmware"
    prefix = "" if official else "PR."
    suffix = "" if official else ".Test"
    identity = f"{repository}.mscoreuefi.{prefix}{flavor['arch']}.{flavor['target']}{suffix}"
    root = ET.Element("package")
    metadata = ET.SubElement(root, "metadata")
    for key, value in {
        "id": identity, "version": f"{base_version()}-{branch}.{commit}",
        "authors": "Microsoft", "description": "Hyper-V UEFI firmware binaries.",
        "owners": "Microsoft",
        "projectUrl": "https://microsoft.visualstudio.com/DefaultCollection/OS/_git/hyperv.uefi/",
        "copyright": "Copyright (C) Microsoft. All rights reserved.",
        "tags": "Microsoft,Hyper-V,UEFI",
    }.items():
        ET.SubElement(metadata, key).text = value
    files = ET.SubElement(root, "files")
    legacy_path = f"Msvm{flavor['arch']}/{flavor['target']}_{flavor['tool_chain']}"
    for path, target in (
        ("FV/MSVM.fd", f"{legacy_path}/FV"), ("PDB/*.pdb", f"symbols/{legacy_path}/PDB"),
        ("MAP/*.map", f"map/{legacy_path}/MAP"),
        ("FwVersion/*", "FwVersion"), ("BuildInfo.json", ""),
    ):
        ET.SubElement(files, "file", src=str(firmware.resolve() / path), target=target)
    output.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(root)
    ET.ElementTree(root).write(output, encoding="utf-8", xml_declaration=True)


class Arguments(argparse.Namespace):
    command: str
    matrix: Path
    source: Path
    output: Path
    commit: str
    build_id: str
    repository: str
    branch: str
    official: bool
    staging_output: Path | None


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("version")
    for command in ("github", "nuget"):
        child = subparsers.add_parser(command)
        child.add_argument("--matrix", required=True, type=Path)
        child.add_argument("--source", required=True, type=Path)
        child.add_argument("--output", required=True, type=Path)
        child.add_argument("--commit", required=True)
        if command == "nuget":
            child.add_argument("--build-id", required=True)
            child.add_argument("--repository", required=True)
            child.add_argument("--branch", required=True)
            child.add_argument("--official", action="store_true")
            child.add_argument("--staging-output", type=Path, help="Stage a compatible OneBranch publication drop")
    args = parser.parse_args(argv, namespace=Arguments())
    try:
        if args.command == "version":
            print(base_version())
        elif args.command == "github":
            release_archives(args.matrix, args.source, args.output, args.commit)
        else:
            nuget_manifest(args.matrix, args.build_id, args.source, args.output,
                           args.commit, args.repository, args.branch, args.official, args.staging_output)
    except (OSError, ValueError, KeyError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()

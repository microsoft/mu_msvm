"""Stage portable firmware outputs and diagnostics; never upload or publish."""

from __future__ import annotations

import argparse
import fnmatch
import json
import shutil
from pathlib import Path

from ci.scripts.matrix import SelectedBuild, select_builds

LOG_PATTERNS = (
    "buildlog*.txt", "buildlog*.md", "build_report.txt", "ci*.txt", "ci*.md",
    "setup*.txt", "update*.txt", "prevallog.txt", "testsuites.xml",
    "build_tools_report.*", "overridelog.txt", "basetools_build*.*", "fd_report.html",
    "ci_buildlog.*", "setuplog.*", "update_log.*",
)


def select_flavor(matrix: Path, identifier: str) -> SelectedBuild:
    for row in select_builds(matrix):
        if row["id"] == identifier:
            return row
    raise ValueError(f"Build ID not present in {matrix}: {identifier}")


def copy_file(source: Path, destination: Path) -> None:
    if not source.is_file() or source.stat().st_size == 0:
        raise ValueError(f"Required artifact is missing or empty: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def copy_outputs(source: Path, destination: Path, suffix: str) -> None:
    files = sorted(source.glob(f"*{suffix}"))
    if not files:
        raise ValueError(f"No {suffix} artifacts found in {source}")
    for file in files:
        copy_file(file, destination / file.name)


def stage_logs(workspace: Path, output: Path) -> None:
    build = workspace / "Build"
    destination = output / "logs"
    destination.mkdir(parents=True, exist_ok=True)
    count = 0
    for file in sorted(build.rglob("*")):
        if file.is_file() and any(fnmatch.fnmatchcase(file.name.lower(), pattern) for pattern in LOG_PATTERNS):
            target = destination / file.relative_to(build)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, target)
            count += 1
    if not count:
        print(f"WARNING: no build logs found under {build}", flush=True)


def source_check_logs(workspace: Path, output: Path) -> None:
    stage_logs(workspace, output)
    package = workspace / "Build/MsvmPkg"
    if package.is_dir():
        shutil.copytree(package, output / "logs/MsvmPkg", dirs_exist_ok=True)


def stage_build(
    workspace: Path, output: Path, flavor: SelectedBuild, succeeded: bool,
    package_inputs: bool = False, full_build_on_failure: bool = False,
) -> None:
    stage_logs(workspace, output)
    build = workspace / "Build" / f"Msvm{flavor['arch']}" / f"{flavor['target']}_{flavor['tool_chain']}"
    if not succeeded:
        if full_build_on_failure:
            if (workspace / "Build").is_dir():
                shutil.copytree(workspace / "Build", output / "all", dirs_exist_ok=True)
            else:
                print(f"WARNING: build failed before Build directory was created: {workspace}", flush=True)
        elif build.is_dir():
            shutil.copytree(build, output / "logs" / "failed-build")
        else:
            print(f"WARNING: build failed before output directory was created: {build}", flush=True)
        return

    firmware = output / "firmware"
    copy_file(build / "FV" / "MSVM.fd", firmware / "FV" / "MSVM.fd")
    for name in ("FirmwareVersion.json", "FwVersionBlob.bin"):
        copy_file(build / "FwVersion" / name, firmware / "FwVersion" / name)
    copy_outputs(build / "MAP", firmware / "MAP", ".map")
    if flavor["tool_chain"] != "GCC":
        copy_outputs(build / "PDB", firmware / "PDB", ".pdb")
    (firmware / "BuildInfo.json").write_text(json.dumps(flavor, indent=2) + "\n", encoding="utf-8")

    if package_inputs:
        package = output / "package" / "Bin"
        copy_file(build / "FV" / "MSVM.fd", package / "MSVM.fd")
        copy_file(build / flavor["arch"] / "MsvmPkg/AcpiTables/AcpiTables/OUTPUT/Dsdt.aml", package / "Dsdt.aml")
        copy_file(workspace / "MsvmPkg/Include/BiosInterface.h", package / "BiosInterface.h")
        for name in ("FirmwareVersion.json", "FwVersionBlob.bin"):
            copy_file(build / "FwVersion" / name, package / name)
        copy_file(firmware / "BuildInfo.json", package / "BuildInfo.json")
        if flavor["tool_chain"] != "GCC":
            copy_outputs(build / "PDB", package / "PDB", ".pdb")


class Arguments(argparse.Namespace):
    output: Path
    matrix: Path | None
    build_id: str | None
    build_succeeded: bool
    logs_only: bool
    package_inputs: bool
    full_build_on_failure: bool
    allow_existing_output: bool


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--matrix", type=Path)
    parser.add_argument("--build-id")
    parser.add_argument("--build-succeeded", action="store_true")
    parser.add_argument("--logs-only", action="store_true")
    parser.add_argument("--package-inputs", action="store_true", help="Collect firmware, ACPI, interface and symbol package inputs")
    parser.add_argument("--full-build-on-failure", action="store_true", help="Retain the entire Build tree after a failure")
    parser.add_argument("--allow-existing-output", action="store_true",
                        help="Allow unrelated destination files without overwriting previously staged payloads")
    args = parser.parse_args(argv, namespace=Arguments())
    workspace = Path.cwd().resolve()
    try:
        output = args.output.resolve()
        if output.is_relative_to(workspace / "Build") or workspace.is_relative_to(output):
            raise ValueError("Artifact destination must be outside Build and must not contain the workspace")
        if output.exists() and any(output.iterdir()) and (
            args.build_succeeded or args.logs_only or not args.full_build_on_failure
        ):
            staged = any((output / name).exists() for name in ("firmware", "logs", "package", "all"))
            if not args.allow_existing_output or staged:
                raise ValueError(f"Artifact destination must not contain existing staged output: {output}")
        if args.logs_only:
            if args.matrix or args.build_id or args.build_succeeded or args.package_inputs or args.full_build_on_failure:
                raise ValueError("--logs-only cannot be combined with build arguments")
            source_check_logs(workspace, output)
        else:
            if args.matrix is None or args.build_id is None:
                raise ValueError("Build staging requires --matrix and --build-id")
            stage_build(workspace, output, select_flavor(args.matrix, args.build_id), args.build_succeeded,
                        args.package_inputs, args.full_build_on_failure)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()

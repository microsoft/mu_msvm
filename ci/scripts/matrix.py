"""Validate repository coverage and emit provider-specific matrix JSON.

This module selects data only: it never builds firmware or authorizes publication.
All rows in the supplied file are validated before host filtering.
Generated IDs identify executions; package suffixes are a
separate consumer-facing contract.
"""

from __future__ import annotations

import argparse
import json
import re
from collections.abc import Mapping
from pathlib import Path
from typing import Literal, NotRequired, TypedDict, cast

Host = Literal["windows", "linux"]
Architecture = Literal["X64", "AARCH64"]
Target = Literal["DEBUG", "RELEASE"]
ToolChain = Literal["VS2022", "CLANGPDB", "GCC"]
Core = Literal["legacy", "patina"]
CompilerSource = Literal["visual_studio", "windows_org", "distribution"]
Debugger = Literal["0", "1"]
OutputFormat = Literal["json", "github", "ado"]


class BuildFlavor(TypedDict):
    """Execution inputs shared by build selection and provider callers."""

    arch: Architecture
    target: Target
    tool_chain: ToolChain
    core: Core
    host: Host
    compiler_source: CompilerSource
    legacy_debugger: Debugger


class BuildDefinition(BuildFlavor):
    """Validated JSON row; optional package metadata must supply both fields."""

    shipping: NotRequired[bool]
    package_suffix: NotRequired[str]


class SelectedBuild(BuildFlavor):
    """Provider row with generated identity and optional closed package metadata.

    Shipping is serialized as text because ADO matrix values are variables.
    It describes eligibility, not permission to publish an official package.
    """

    id: str
    shipping: NotRequired[str]
    package_suffix: NotRequired[str]


class MatrixArguments(argparse.Namespace):
    """CLI values constrained by argparse choices before selection."""

    matrix: Path
    host: Host | None
    format: OutputFormat

FLAVOR_CHOICES: dict[str, tuple[str, ...]] = {
    "arch": ("X64", "AARCH64"),
    "target": ("DEBUG", "RELEASE"),
    "tool_chain": ("VS2022", "CLANGPDB", "GCC"),
    "core": ("legacy", "patina"),
}
EXECUTION_CHOICES: dict[str, tuple[str, ...]] = {
    "host": ("windows", "linux"),
    "compiler_source": ("visual_studio", "windows_org", "distribution"),
    "legacy_debugger": ("0", "1"),
}


def build_id(row: BuildFlavor) -> str:
    """Derive a stable execution ID, independent of package names or repo mode."""
    return "_".join((row["host"], row["arch"], row["target"], row["tool_chain"],
                     row["compiler_source"], row["core"], row["legacy_debugger"]))


def parse_definition(value: object) -> BuildDefinition:
    """Validate one untrusted JSON row before admitting it to the typed model.

    Raises:
        ValueError: Missing/unknown fields, invalid scalar types or choices,
            unsupported compilers, or inconsistent package metadata.
    """
    if not isinstance(value, dict):
        raise ValueError("Each build must specify a complete execution flavor")
    row = cast(dict[str, object], value)
    required = {*FLAVOR_CHOICES, *EXECUTION_CHOICES}
    if not required <= row.keys() or row.keys() - required - {"shipping", "package_suffix"}:
        raise ValueError("Each build must specify a complete execution flavor")
    for field, choices in {**FLAVOR_CHOICES, **EXECUTION_CHOICES}.items():
        if not isinstance(row[field], str) or row[field] not in choices:
            raise ValueError(f"Invalid {field}: {row[field]!r}")
    valid_compilers = {
        ("windows", "VS2022", "visual_studio"),
        ("windows", "CLANGPDB", "visual_studio"),
        ("windows", "CLANGPDB", "windows_org"),
        ("linux", "CLANGPDB", "distribution"),
        ("linux", "GCC", "distribution"),
    }
    if (row["host"], row["tool_chain"], row["compiler_source"]) not in valid_compilers:
        raise ValueError("Unsupported host/compiler combination")
    if row["arch"] == "AARCH64" and row["tool_chain"] == "VS2022":
        raise ValueError("ARM64 MSVC is not supported")
    if row["legacy_debugger"] == "1" and (
        row["arch"] != "X64" or row["core"] != "legacy" or row["tool_chain"] not in ("VS2022", "CLANGPDB")
    ):
        raise ValueError("Legacy debugger requires X64 VS2022 or CLANGPDB with the legacy core")
    if "shipping" in row or "package_suffix" in row:
        suffix = row.get("package_suffix")
        if type(row.get("shipping")) is not bool or not isinstance(suffix, str):
            raise ValueError("Package metadata requires explicit shipping and package_suffix")
        if not re.fullmatch(r"[A-Za-z0-9]*", suffix) or row["shipping"] != (suffix == ""):
            raise ValueError("Invalid shipping/package suffix contract")
    return cast(BuildDefinition, row)


def select_builds(path: Path, host: str | None = None) -> list[SelectedBuild]:
    """Load the explicitly supplied matrix file, optionally restricted by host.

    Preserves declaration order and rejects duplicate execution/package IDs.
    Relative paths resolve against the caller's working directory. No repository
    or CI backend is inferred from the path; either file can be used locally.
    Raises OSError for unreadable input and ValueError for invalid JSON, invalid
    definitions, unknown filters, or an empty selection. No commands are run.
    """
    if host is not None and host not in EXECUTION_CHOICES["host"]:
        raise ValueError(f"Unknown host: {host}")
    document: object = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(document, list):
        raise ValueError("Build matrix must be a list")
    selected: list[SelectedBuild] = []
    identifiers: set[str] = set()
    package_names: set[tuple[Architecture, Target, str]] = set()
    for value in cast(list[object], document):
        row = parse_definition(value)
        identifier = build_id(row)
        if identifier in identifiers:
            raise ValueError(f"Duplicate matrix identifier: {identifier}")
        identifiers.add(identifier)
        if "package_suffix" in row:
            suffix = row["package_suffix"]
            package_name = (row["arch"], row["target"], suffix)
            if package_name in package_names:
                raise ValueError(f"Duplicate closed package identity: {package_name}")
            package_names.add(package_name)
        if host is None or row["host"] == host:
            selected_row: SelectedBuild = {
                "id": identifier,
                "arch": row["arch"], "target": row["target"], "tool_chain": row["tool_chain"],
                "core": row["core"], "host": row["host"], "compiler_source": row["compiler_source"],
                "legacy_debugger": row["legacy_debugger"],
            }
            if "shipping" in row:
                selected_row["shipping"] = str(row["shipping"]).lower()
                selected_row["package_suffix"] = row["package_suffix"]
            selected.append(selected_row)
    if not selected:
        raise ValueError(f"No builds selected from {path} for host: {host or 'all'}")
    return selected


def main(argv: list[str] | None = None) -> None:
    """Print compact matrix JSON; argparse reports input failures with exit 2."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", required=True, type=Path, help="Path to the build coverage JSON file")
    parser.add_argument("--host", choices=EXECUTION_CHOICES["host"])
    parser.add_argument("--format", choices=("json", "github", "ado"), default="json")
    args = parser.parse_args(argv, namespace=MatrixArguments())
    try:
        rows = select_builds(args.matrix, host=args.host)
        if args.format == "github" and any(row["legacy_debugger"] == "1" for row in rows):
            raise ValueError("GitHub builds cannot enable the closed-source legacy debugger")
    except (OSError, ValueError) as error:
        parser.error(str(error))
    result: object
    if args.format == "github":
        result = {"include": rows}
    elif args.format == "ado":
        result = {row["id"]: {key: value for key, value in cast(Mapping[str, str], row).items() if key != "id"}
              for row in rows}
    else:
        result = rows
    print(json.dumps(result, separators=(",", ":")))


if __name__ == "__main__":
    main()

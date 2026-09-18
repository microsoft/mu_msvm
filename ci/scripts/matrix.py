"""Select explicit repository build coverage and emit CI-provider matrix data."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

MATRIX_PATH = Path(__file__).resolve().parents[1] / "build-matrix.json"
REPOSITORIES = ("open", "closed")
FLAVOR_CHOICES = {
    "arch": ("X64", "AARCH64"),
    "target": ("DEBUG", "RELEASE"),
    "tool_chain": ("VS2022", "CLANGPDB", "GCC"),
    "core": ("legacy", "patina"),
}
EXECUTION_CHOICES = {
    "host": ("windows", "linux"),
    "compiler_source": ("visual_studio", "windows_org", "distribution"),
    "legacy_debugger": ("0", "1"),
}


def build_id(row: dict) -> str:
    return "_".join(row[field] for field in (
        "host", "arch", "target", "tool_chain", "compiler_source", "core", "legacy_debugger",
    ))


def select_builds(repository: str, path: Path = MATRIX_PATH, host: str | None = None) -> list[dict]:
    if repository not in REPOSITORIES:
        raise ValueError(f"Unknown repository mode: {repository}")
    if host is not None and host not in EXECUTION_CHOICES["host"]:
        raise ValueError(f"Unknown host: {host}")
    rows = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(rows, list):
        raise ValueError("Build matrix must be a list")
    selected = []
    identifiers = set()
    package_names = set()
    for row in rows:
        required = {"repositories", *FLAVOR_CHOICES, *EXECUTION_CHOICES}
        if not isinstance(row, dict) or not required <= row.keys() or row.keys() - required - {"shipping", "package_suffix"}:
            raise ValueError("Each build must specify repository coverage and a complete execution flavor")
        for field, choices in {**FLAVOR_CHOICES, **EXECUTION_CHOICES}.items():
            if row[field] not in choices:
                raise ValueError(f"Invalid {field}: {row[field]!r}")
        identifier = build_id(row)
        valid_compilers = {
            ("windows", "VS2022", "visual_studio"),
            ("windows", "CLANGPDB", "visual_studio"),
            ("windows", "CLANGPDB", "windows_org"),
            ("linux", "CLANGPDB", "distribution"),
            ("linux", "GCC", "distribution"),
        }
        if (row["host"], row["tool_chain"], row["compiler_source"]) not in valid_compilers:
            raise ValueError(f"Unsupported host/compiler combination: {identifier}")
        if row["arch"] == "AARCH64" and row["tool_chain"] == "VS2022":
            raise ValueError("ARM64 MSVC is not supported")
        repositories = row["repositories"]
        if not isinstance(repositories, list) or not repositories or any(mode not in REPOSITORIES for mode in repositories):
            raise ValueError(f"Invalid repositories for {identifier}")
        for mode in repositories:
            if (mode, identifier) in identifiers:
                raise ValueError(f"Duplicate matrix identifier for {mode}: {identifier}")
            identifiers.add((mode, identifier))
        if "closed" in repositories:
            if type(row.get("shipping")) is not bool or not isinstance(row.get("package_suffix"), str):
                raise ValueError(f"Closed builds require explicit shipping and package_suffix: {identifier}")
            suffix = row["package_suffix"]
            if not re.fullmatch(r"[A-Za-z0-9]*", suffix) or row["shipping"] != (suffix == ""):
                raise ValueError(f"Invalid shipping/package suffix contract: {identifier}")
            package_name = (row["arch"], row["target"], suffix)
            if package_name in package_names:
                raise ValueError(f"Duplicate closed package identity: {package_name}")
            package_names.add(package_name)
        elif "shipping" in row or "package_suffix" in row:
            raise ValueError("Closed package metadata must belong to closed repository coverage")
        if repository in repositories and (host is None or row["host"] == host):
            selected.append({
                "id": identifier,
                **{field: row[field] for field in (*FLAVOR_CHOICES, *EXECUTION_CHOICES)},
                **({"shipping": str(row["shipping"]).lower(), "package_suffix": row["package_suffix"]}
                   if repository == "closed" else {}),
            })
    if not selected:
        raise ValueError(f"No builds configured for repository mode: {repository}")
    return selected


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, choices=REPOSITORIES)
    parser.add_argument("--host", choices=EXECUTION_CHOICES["host"])
    parser.add_argument("--format", choices=("json", "github", "ado"), default="json")
    args = parser.parse_args(argv)
    try:
        rows = select_builds(args.repo, host=args.host)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    if args.format == "github":
        result = {"include": rows}
    elif args.format == "ado":
        result = {row["id"]: {key: value for key, value in row.items() if key != "id"} for row in rows}
    else:
        result = rows
    print(json.dumps(result, separators=(",", ":")))


if __name__ == "__main__":
    main()

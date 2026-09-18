"""Select explicit repository build coverage and emit CI-provider matrix data."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

MATRIX_PATH = Path(__file__).resolve().parents[1] / "build-matrix.json"
REPOSITORIES = ("open", "closed")
FLAVOR_CHOICES = {
    "arch": ("X64", "AARCH64"),
    "target": ("DEBUG", "RELEASE"),
    "tool_chain": ("VS2022", "CLANGPDB", "GCC"),
    "core": ("legacy", "patina"),
}


def select_builds(repository: str, path: Path = MATRIX_PATH) -> list[dict]:
    if repository not in REPOSITORIES:
        raise ValueError(f"Unknown repository mode: {repository}")
    rows = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(rows, list):
        raise ValueError("Build matrix must be a list")
    selected = []
    identifiers = set()
    for row in rows:
        if not isinstance(row, dict) or set(row) != {"repositories", *FLAVOR_CHOICES}:
            raise ValueError("Each build must specify repositories, arch, target, tool_chain, and core")
        for field, choices in FLAVOR_CHOICES.items():
            if row[field] not in choices:
                raise ValueError(f"Invalid {field}: {row[field]!r}")
        identifier = "_".join(row[field] for field in FLAVOR_CHOICES)
        if identifier in identifiers:
            raise ValueError(f"Duplicate matrix identifier: {identifier}")
        identifiers.add(identifier)
        repositories = row["repositories"]
        if not isinstance(repositories, list) or not repositories or any(mode not in REPOSITORIES for mode in repositories):
            raise ValueError(f"Invalid repositories for {identifier}")
        if repository in repositories:
            selected.append({"id": identifier, **{field: row[field] for field in FLAVOR_CHOICES}})
    if not selected:
        raise ValueError(f"No builds configured for repository mode: {repository}")
    return selected


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, choices=REPOSITORIES)
    parser.add_argument("--format", choices=("json", "github", "ado"), default="json")
    args = parser.parse_args(argv)
    try:
        rows = select_builds(args.repo)
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

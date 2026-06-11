"""
Compute the next release tag.

Logic:
    1. Load baseVersion + tagPrefix from data/release.yml.
    2. Query existing GitHub releases via `gh release list`.
    3. Find the highest patch number with tag prefix "<tagPrefix><baseVersion>."
       (e.g. "v26.0."). If none exist, patch = 0.
    4. Otherwise patch = max_patch + 1.
    5. Print "tag=<tagPrefix><baseVersion>.<patch>" so the workflow can capture
       it via `>> "$GITHUB_OUTPUT"`.

Requires `gh` CLI to be installed and authenticated (GH_TOKEN env var on CI).

Usage in workflow:
    - id: compute_tag
      run: python .github/workflows/CreateRelease/scripts/compute_version.py \\
            --repo ${{ github.repository }} >> "$GITHUB_OUTPUT"
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys

from release_common import die, load_release_config


def _list_existing_tags(repo: str) -> list[str]:
    result = subprocess.run(
        ["gh", "release", "list", "--repo", repo, "--limit", "1000", "--json", "tagName"],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        die(f"`gh release list` failed: {result.stderr}")
    data = json.loads(result.stdout or "[]")
    return [item["tagName"] for item in data if "tagName" in item]


def _next_patch(existing: list[str], prefix: str) -> int:
    max_patch = -1
    for tag in existing:
        if not tag.startswith(prefix):
            continue
        suffix = tag[len(prefix):]
        try:
            patch = int(suffix)
        except ValueError:
            continue
        if patch > max_patch:
            max_patch = patch
    return max_patch + 1


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--repo", required=True,
                        help="GitHub repo in 'owner/name' form (e.g. github.repository).")
    parser.add_argument("--dry-run", action="store_true",
                        help="Don't shell out to gh; emit patch=0 (for tests).")
    args = parser.parse_args()

    cfg = load_release_config()
    base_version: str = str(cfg["baseVersion"])
    tag_prefix: str = str(cfg["tagPrefix"])
    full_prefix = f"{tag_prefix}{base_version}."

    if args.dry_run:
        patch = 0
    else:
        existing = _list_existing_tags(args.repo)
        patch = _next_patch(existing, full_prefix)

    tag = f"{full_prefix}{patch}"
    print(f"tag={tag}")
    print(f"baseVersion={base_version}", file=sys.stderr)
    print(f"patch={patch}", file=sys.stderr)


if __name__ == "__main__":
    main()

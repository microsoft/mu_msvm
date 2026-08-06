"""
Package downloaded build artifacts into release tarballs.

Each top-level subdirectory under --artifacts-dir is treated as one logical
artifact (matching actions/download-artifact's per-artifact subdir layout) and
gets archived as `<release-dir>/<artifact-name>.tar.gz`.

Usage:
    python .github/workflows/CreateRelease/scripts/package_artifacts.py \\
        --artifacts-dir artifacts \\
        --release-dir release
"""

from __future__ import annotations

import argparse
import shutil
import tarfile
from pathlib import Path

from release_common import die


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--artifacts-dir", required=True, type=Path,
                        help="Root dir containing per-artifact subdirectories.")
    parser.add_argument("--release-dir", required=True, type=Path,
                        help="Destination dir for *.tar.gz outputs.")
    args = parser.parse_args()

    if not args.artifacts_dir.is_dir():
        die(f"--artifacts-dir not found or not a directory: {args.artifacts_dir}")

    release_dir = args.release_dir.resolve()
    if release_dir.exists():
        shutil.rmtree(release_dir)
    release_dir.mkdir(parents=True)

    packaged: list[Path] = []
    for sub in sorted(p for p in args.artifacts_dir.iterdir() if p.is_dir()):
        tar_path = release_dir / f"{sub.name}.tar.gz"
        print(f"  packing {sub.name} -> {tar_path}")
        with tarfile.open(tar_path, "w:gz") as tf:
            for child in sorted(sub.iterdir()):
                tf.add(child, arcname=child.name)
        packaged.append(tar_path)

    if not packaged:
        die(f"No artifact subdirectories found under {args.artifacts_dir}")

    print(f"\nPackaged {len(packaged)} artifact(s) into {release_dir}:")
    for p in packaged:
        print(f"  {p}")


if __name__ == "__main__":
    main()

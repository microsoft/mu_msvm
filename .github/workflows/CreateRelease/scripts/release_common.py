"""Shared types and helpers for the CreateRelease pipeline."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

# This file lives at: <repo>/.github/workflows/CreateRelease/scripts/release_common.py
SCRIPTS_DIR = Path(__file__).resolve().parent
RELEASE_PIPELINE_DIR = SCRIPTS_DIR.parent
DATA_DIR = RELEASE_PIPELINE_DIR / "data"
REPO_ROOT = RELEASE_PIPELINE_DIR.parent.parent.parent


def die(msg: str) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def run(label: str, cmd: list[str], **kwargs) -> subprocess.CompletedProcess:
    print(f"== {label} ==", flush=True)
    return subprocess.run(cmd, check=True, **kwargs)


def load_release_config() -> dict:
    """Load and validate data/release.yml. Requires PyYAML."""
    try:
        import yaml  # type: ignore
    except ImportError:
        die("PyYAML required to load release.yml. pip install pyyaml.")

    release_file = DATA_DIR / "release.yml"
    if not release_file.exists():
        die(f"release.yml not found at: {release_file}")
    data = yaml.safe_load(release_file.read_text())
    if not isinstance(data, dict):
        die(f"release.yml is malformed: {release_file}")
    for key in ("baseVersion", "tagPrefix"):
        if key not in data:
            die(f"release.yml missing '{key}'")
    return data


def load_release_config() -> dict:
    """Load and validate data/release.yml. Requires PyYAML."""
    try:
        import yaml  # type: ignore
    except ImportError:
        die("PyYAML required to load release.yml. pip install pyyaml.")

    release_file = DATA_DIR / "release.yml"
    if not release_file.exists():
        die(f"release.yml not found at: {release_file}")
    data = yaml.safe_load(release_file.read_text())
    if not isinstance(data, dict):
        die(f"release.yml is malformed: {release_file}")
    for key in ("baseVersion", "tagPrefix"):
        if key not in data:
            die(f"release.yml missing '{key}'")
    return data

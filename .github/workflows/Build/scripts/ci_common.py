"""Shared types and helpers for the mu_msvm Build pipeline scripts."""

from __future__ import annotations

import subprocess
import sys
import shlex
from pathlib import Path
from typing import Literal

# ---------------------------------------------------------------------------
# Canonical value sets -- runtime choices for argparse and type aliases.
# ---------------------------------------------------------------------------

Arch = Literal["X64", "AARCH64"]
Target = Literal["DEBUG", "RELEASE"]
ToolChainTag = Literal["VS2022", "CLANGPDB"]

VALID_ARCHS: tuple[str, ...] = ("X64", "AARCH64")
VALID_TARGETS: tuple[str, ...] = ("DEBUG", "RELEASE")
VALID_TOOLCHAINS: tuple[str, ...] = ("VS2022", "CLANGPDB")

# ---------------------------------------------------------------------------
# Layout helpers
# ---------------------------------------------------------------------------

# This file lives at: <repo>/.github/workflows/Build/scripts/ci_common.py
SCRIPTS_DIR = Path(__file__).resolve().parent
BUILD_PIPELINE_DIR = SCRIPTS_DIR.parent
DATA_DIR = BUILD_PIPELINE_DIR / "data"
REPO_ROOT = BUILD_PIPELINE_DIR.parent.parent.parent

DEFAULT_PLATFORM_PKG = "MsvmPkg"


def build_dir_for(arch: Arch, target: Target, tools: ToolChainTag) -> Path:
    """Return the Stuart build output directory for a variant (relative to repo root)."""
    return Path("Build") / f"Msvm{arch}" / f"{target}_{tools}"


# ---------------------------------------------------------------------------
# Process helpers
# ---------------------------------------------------------------------------


def format_command(cmd: list[str]) -> str:
    """Return a shell-readable command line for logging."""
    return subprocess.list2cmdline(cmd) if sys.platform == "win32" else shlex.join(cmd)


def run(label: str, cmd: list[str], **kwargs) -> None:
    """Print *label* and exact command, execute it, and fail on non-zero exit."""
    print(f"== {label} ==", flush=True)
    print(f"> {format_command(cmd)}", flush=True)
    subprocess.run(cmd, check=True, **kwargs)


def die(msg: str) -> None:
    """Print *msg* to stderr and exit 1."""
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def load_variants() -> list[dict]:
    """Load the variant list from data/matrix.yml. Requires PyYAML."""
    try:
        import yaml  # type: ignore
    except ImportError:
        die("PyYAML required to load matrix.yml. pip install pyyaml.")

    variants_file = DATA_DIR / "matrix.yml"
    if not variants_file.exists():
        die(f"matrix.yml not found at: {variants_file}")

    data = yaml.safe_load(variants_file.read_text())
    variants = data.get("variants") if isinstance(data, dict) else None
    if not isinstance(variants, list) or not variants:
        die(f"matrix.yml has no 'variants' list: {variants_file}")

    for i, v in enumerate(variants):
        for key in ("arch", "target", "tools"):
            if key not in v:
                die(f"matrix.yml entry {i} missing '{key}': {v}")
    return variants

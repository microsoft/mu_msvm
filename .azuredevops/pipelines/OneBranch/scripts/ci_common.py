"""Shared types and helpers for Hyper-V UEFI CI scripts."""

from __future__ import annotations

import subprocess
import sys
from typing import Literal

# ---------------------------------------------------------------------------
# Canonical value sets — used both as runtime choices in argparse and as
# type aliases for static analysis.
# ---------------------------------------------------------------------------

Arch = Literal["X64", "AARCH64"]
Target = Literal["DEBUG", "RELEASE"]
ToolChainTag = Literal["VS2022", "CLANGPDB", "GCC"]
Pool = Literal["Windows", "Linux"]
Pipeline = Literal["Official", "PR"]

VALID_ARCHS: tuple[str, ...] = ("X64", "AARCH64")
VALID_TARGETS: tuple[str, ...] = ("DEBUG", "RELEASE")
VALID_TOOLCHAINS: tuple[str, ...] = ("VS2022", "CLANGPDB", "GCC")
VALID_POOLS: tuple[str, ...] = ("Windows", "Linux")
VALID_PIPELINES: tuple[str, ...] = ("Official", "PR")


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def run(label: str, cmd: list[str]) -> None:
    """Print *label*, execute *cmd*, and raise CalledProcessError on failure."""
    print(f"== {label} ==")
    subprocess.run(cmd, check=True)


def die(msg: str) -> None:
    """Print *msg* to stderr and exit 1."""
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)

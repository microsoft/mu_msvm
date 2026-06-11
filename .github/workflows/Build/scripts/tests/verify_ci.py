"""
Local verification sweep for the Build pipeline.

Runs every check that can be done without a CI runner:
    1. YAML parse       -- every *.yml / *.yaml under Build/
    2. Python syntax    -- every *.py under scripts/
    3. pytest           -- unit tests in tests/
    4. Variant cross-check  -- generate_matrix output matches matrix.yml

Usage:
    python .github/workflows/Build/scripts/tests/verify_ci.py
"""

from __future__ import annotations

import json
import py_compile
import subprocess
import sys
from pathlib import Path

_TESTS_DIR = Path(__file__).resolve().parent
_SCRIPTS_DIR = _TESTS_DIR.parent
_BUILD_PIPELINE_DIR = _SCRIPTS_DIR.parent
_REPO_ROOT = _BUILD_PIPELINE_DIR.parent.parent.parent

sys.path.insert(0, str(_SCRIPTS_DIR))

_results: dict[str, bool] = {}


def _header(title: str) -> None:
    print(f"\n{'=' * 60}\n  {title}\n{'=' * 60}")


def _section_yaml_parse() -> bool:
    _header("1. YAML parse")
    try:
        import yaml  # type: ignore
    except ImportError:
        print("SKIP: pyyaml not installed (pip install pyyaml).")
        return True
    fail = 0
    for f in sorted(_BUILD_PIPELINE_DIR.rglob("*.yml")) + \
             sorted(_BUILD_PIPELINE_DIR.rglob("*.yaml")):
        try:
            yaml.safe_load(f.read_text())
            print(f"  PASS: {f.relative_to(_REPO_ROOT)}")
        except Exception as exc:  # noqa: BLE001
            print(f"  FAIL: {f.relative_to(_REPO_ROOT)} -- {exc}")
            fail += 1
    return fail == 0


def _section_python_syntax() -> bool:
    _header("2. Python syntax")
    fail = 0
    for f in sorted(_SCRIPTS_DIR.rglob("*.py")):
        try:
            py_compile.compile(str(f), doraise=True)
            print(f"  PASS: {f.relative_to(_REPO_ROOT)}")
        except py_compile.PyCompileError as exc:
            print(f"  FAIL: {f.relative_to(_REPO_ROOT)} -- {exc}")
            fail += 1
    return fail == 0


def _section_pytest() -> bool:
    _header("3. pytest")
    try:
        import pytest  # noqa: F401
    except ImportError:
        print("SKIP: pytest not installed (pip install pytest).")
        return True
    result = subprocess.run(
        [sys.executable, "-m", "pytest", str(_TESTS_DIR), "-v"],
    )
    return result.returncode == 0


def _section_matrix() -> bool:
    _header("4. generate_matrix vs matrix.yml")
    try:
        from ci_common import load_variants  # noqa: PLC0415
    except ImportError as exc:
        print(f"SKIP: cannot import ci_common: {exc}")
        return True
    result = subprocess.run(
        [sys.executable, str(_SCRIPTS_DIR / "generate_matrix.py")],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        print(f"FAIL: generate_matrix.py exited {result.returncode}\n{result.stderr}")
        return False
    line = result.stdout.strip()
    if not line.startswith("matrix="):
        print(f"FAIL: missing matrix= prefix: {line!r}")
        return False
    matrix = json.loads(line[len("matrix="):])
    variants = load_variants()
    if len(matrix["include"]) != len(variants):
        print(f"FAIL: include length {len(matrix['include'])} != variants {len(variants)}")
        return False
    print(f"  PASS: {len(variants)} variants match")
    return True


def main() -> int:
    sections = [
        ("yaml_parse", _section_yaml_parse),
        ("python_syntax", _section_python_syntax),
        ("pytest", _section_pytest),
        ("matrix_parity", _section_matrix),
    ]
    for name, fn in sections:
        _results[name] = fn()

    _header("Summary")
    for name, ok in _results.items():
        print(f"  {'PASS' if ok else 'FAIL'}: {name}")
    return 0 if all(_results.values()) else 1


if __name__ == "__main__":
    sys.exit(main())

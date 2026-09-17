"""
Local verification sweep for the Hyper-V UEFI CI pipeline scripts.

Runs every check that can be done without an ADO runner:
    1. YAML parse       -- every *.yml / *.yaml under OneBranch/
    2. Python syntax    -- every *.py under scripts/
    3. mypy             -- type-check scripts/ (skipped if mypy not installed)
    4. pytest           -- unit tests in tests/
    5. CLI validation   -- each main script rejects missing required args (exit 1 or 2)
    6. Vpack-name parity -- jobs.yml/CI-Build.yaml/PR-Build.yaml vpack composition
                           definitions + data/matrix.yml agree with get_vpack_name()
    7. F401 lint         -- optional dead-import check via ruff (skipped if unavailable)

Each section prints PASS / FAIL / SKIP. Exits 0 only if all sections pass.

NOTE: This script does not validate the ADO pipeline itself. To verify the
full expanded pipeline, queue a PR build and inspect the YAML in the ADO UI.

Usage:
    python .azuredevops/pipelines/OneBranch/scripts/tests/verify_ci.py
"""

from __future__ import annotations

import py_compile
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Directory layout (relative to this file's position)
# ---------------------------------------------------------------------------

_TESTS_DIR = Path(__file__).parent                   # .../scripts/tests/
_SCRIPTS_DIR = _TESTS_DIR.parent                     # .../scripts/
_ONEBRANCH_DIR = _SCRIPTS_DIR.parent                 # .../OneBranch/
_REPO_ROOT = _ONEBRANCH_DIR.parent.parent.parent     # workspace root

sys.path.insert(0, str(_SCRIPTS_DIR))

# ---------------------------------------------------------------------------
# Section helpers
# ---------------------------------------------------------------------------

_results: dict[str, bool] = {}


def _header(title: str) -> None:
    print(f"\n{'=' * 60}")
    print(f"  {title}")
    print("=" * 60)


# ---------------------------------------------------------------------------
# YAML parse
# ---------------------------------------------------------------------------


def _section_yaml_parse() -> bool:
    _header("YAML parse")
    try:
        import yaml  # noqa: PLC0415
    except ImportError:
        print("SKIP: pyyaml not installed (run: pip install pyyaml).")
        return True  # non-blocking

    fail = 0
    for f in sorted(_ONEBRANCH_DIR.rglob("*.yml")) + sorted(_ONEBRANCH_DIR.rglob("*.yaml")):
        try:
            yaml.safe_load(f.read_text())
            print(f"  PASS: {f.relative_to(_REPO_ROOT)}")
        except Exception as exc:
            print(f"  FAIL: {f.relative_to(_REPO_ROOT)} -- {exc}")
            fail += 1
    return fail == 0


# ---------------------------------------------------------------------------
# Python syntax
# ---------------------------------------------------------------------------


def _section_python_syntax() -> bool:
    _header("Python syntax (py_compile)")
    fail = 0
    for f in sorted(_SCRIPTS_DIR.rglob("*.py")):
        rel = f.relative_to(_REPO_ROOT)
        try:
            py_compile.compile(str(f), doraise=True)
            print(f"  PASS: {rel}")
        except py_compile.PyCompileError as exc:
            print(f"  FAIL: {rel} -- {exc}")
            fail += 1
    return fail == 0


# ---------------------------------------------------------------------------
# mypy type check
# ---------------------------------------------------------------------------


def _section_mypy() -> bool:
    _header("mypy type check")
    try:
        result = subprocess.run(
            ["mypy", "--ignore-missing-imports", str(_SCRIPTS_DIR)],
            capture_output=True,
            text=True,
        )
    except FileNotFoundError:
        print("  SKIP: mypy not installed (run: pip install mypy)")
        return True  # non-blocking
    if result.returncode == 0:
        print("  PASS: 0 type errors")
        return True
    if "No module named mypy" in (result.stderr + result.stdout):
        print("  SKIP: mypy not installed (run: pip install mypy)")
        return True  # non-blocking
    print("  FAIL: mypy reported errors:")
    for line in result.stdout.splitlines():
        print(f"    {line}")
    return False


# ---------------------------------------------------------------------------
# pytest unit tests
# ---------------------------------------------------------------------------


def _section_pytest() -> bool:
    _header("pytest unit tests")
    result = subprocess.run(
        [sys.executable, "-m", "pytest", str(_TESTS_DIR), "-v", "--tb=short"],
        cwd=_REPO_ROOT,
    )
    return result.returncode == 0


# ---------------------------------------------------------------------------
# CLI validation
# ---------------------------------------------------------------------------


def _section_cli_validation() -> bool:
    _header("CLI validation (scripts must reject missing required args)")
    cases = [
        "set_repo_auth.py",
        "install_toolchain.py",
        "build.py",
        "stage_artifacts.py",
        "get_vpack_name.py",
    ]
    fail = 0
    for script_name in cases:
        result = subprocess.run(
            [sys.executable, str(_SCRIPTS_DIR / script_name)],
            capture_output=True,
        )
        # argparse exits 2 for missing required args; sys.exit(1) exits 1.
        if result.returncode in (1, 2):
            print(f"  PASS: {script_name} rejects missing required args (exit {result.returncode})")
        else:
            print(f"  FAIL: {script_name} accepted empty args (exit {result.returncode})")
            fail += 1
    return fail == 0


# ---------------------------------------------------------------------------
# Vpack-name parity
# ---------------------------------------------------------------------------


def _section_vpack_parity() -> bool:
    _header("Vpack-name parity (matrix.yml inline composition vs. get_vpack_name.py)")
    try:
        import yaml  # noqa: PLC0415
    except ImportError:
        print("  SKIP: pyyaml not installed.")
        return True

    from get_vpack_name import get_vpack_name  # noqa: PLC0415

    jobs_text = (_ONEBRANCH_DIR / "jobs.yml").read_text()
    ci_yaml = yaml.safe_load((_ONEBRANCH_DIR / "CI-Build.yaml").read_text())
    pr_yaml = yaml.safe_load((_ONEBRANCH_DIR / "PR-Build.yaml").read_text())
    matrix_path = _ONEBRANCH_DIR / "data" / "matrix.yml"
    matrix = yaml.safe_load(matrix_path.read_text())
    builds_param = next(p for p in matrix["parameters"] if p["name"] == "Builds")
    builds: list[dict] = builds_param["default"]

    repo_name = "MsvmPkgX64"
    fail = 0
    total = 0

    # Assert the YAML sources still use the expected composition primitives,
    # so this check fails if the pipeline expression drifts from the Python impl.
    expected_dot_salt = "${{iif(eq(Build.Salt, ''), '', format('.{0}', Build.Salt))}}"
    expected_dot_test = "${{iif(or(ne(Build.Salt, ''), eq(parameters.pipeline, 'PR')), '.Test', '')}}"
    expected_packagename = "$(Build.Repository.Name).mscoreuefi.$(PRdot)${{Build.Arch}}.${{Build.Target}}$(DotSalt)$(DotTest)"
    for label, needle in (
        ("DotSalt expression", expected_dot_salt),
        ("DotTest expression", expected_dot_test),
        ("ob_createvpack_packagename", expected_packagename),
    ):
        if needle not in jobs_text:
            print(f"  FAIL: jobs.yml missing expected {label}: {needle}")
            return False

    for row in builds:
        extra = str(row.get("Extra", ""))
        if "BLD_*_USE_LEGACY_C_CORE=TRUE" not in extra or "BLD_*_USE_LEGACY_C_CORE=FALSE" in extra:
            print(f"  FAIL: matrix row has incorrect core selection: {row}")
            return False

    def _get_prdot(doc: dict) -> str:
        for entry in doc.get("variables", []):
            if isinstance(entry, dict) and entry.get("name") == "PRdot":
                return str(entry.get("value", ""))
        raise ValueError("PRdot variable not found")

    try:
        prdot_by_pipeline = {
            "Official": _get_prdot(ci_yaml),
            "PR": _get_prdot(pr_yaml),
        }
    except ValueError as exc:
        print(f"  FAIL: {exc}")
        return False

    if prdot_by_pipeline["Official"] != "":
        print(f"  FAIL: CI-Build.yaml PRdot expected '' but found {prdot_by_pipeline['Official']!r}")
        return False
    if prdot_by_pipeline["PR"] != "PR.":
        print(f"  FAIL: PR-Build.yaml PRdot expected 'PR.' but found {prdot_by_pipeline['PR']!r}")
        return False

    for row in builds:
        for pipeline in ("Official", "PR"):
            total += 1
            salt = str(row.get("Salt", ""))
            arch = row["Arch"]
            target = row["Target"]

            # Recompose using values sourced from pipeline YAML definitions.
            pr_dot = prdot_by_pipeline[pipeline]
            dot_salt = f".{salt}" if salt else ""
            dot_test = ".Test" if (salt or pipeline == "PR") else ""
            yaml_name = f"{repo_name}.mscoreuefi.{pr_dot}{arch}.{target}{dot_salt}{dot_test}"

            spec_name = get_vpack_name(repo_name, pipeline, arch, target, salt)  # type: ignore[arg-type]

            if yaml_name == spec_name:
                print(f"  PASS: {yaml_name}")
            else:
                print(f"  FAIL: yaml_name={yaml_name!r}  spec_name={spec_name!r}")
                fail += 1

    print(f"\n  {total - fail}/{total} parity checks passed.")
    return fail == 0


# ---------------------------------------------------------------------------
# F401 lint (optional)
# ---------------------------------------------------------------------------


def _section_f401_lint() -> bool:
    _header("F401 lint (ruff optional)")
    try:
        result = subprocess.run(
            [sys.executable, "-m", "ruff", "check", "--select", "F401", str(_SCRIPTS_DIR)],
            capture_output=True,
            text=True,
        )
    except FileNotFoundError:
        print("  SKIP: python executable not found")
        return True

    if result.returncode == 0:
        print("  PASS: no unused imports")
        return True

    stderr_stdout = (result.stderr + result.stdout).lower()
    if "no module named ruff" in stderr_stdout:
        print("  SKIP: ruff not installed (run: pip install ruff)")
        return True

    print("  FAIL: ruff found issues:")
    for line in result.stdout.splitlines():
        print(f"    {line}")
    return False


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------


def main() -> None:
    sections = [
        ("YAML parse", _section_yaml_parse),
        ("Python syntax", _section_python_syntax),
        ("mypy", _section_mypy),
        ("pytest", _section_pytest),
        ("CLI validation", _section_cli_validation),
        ("Vpack-name parity", _section_vpack_parity),
        ("F401 lint", _section_f401_lint),
    ]

    for title, fn in sections:
        _results[title] = fn()

    print(f"\n{'=' * 60}")
    print("  Overall")
    print("=" * 60)
    all_pass = True
    for name, passed in _results.items():
        status = "PASS" if passed else "FAIL"
        print(f"  {status}: {name}")
        if not passed:
            all_pass = False

    if all_pass:
        print("\nAll local verification sections PASSED.")
        sys.exit(0)
    else:
        print("\nOne or more sections FAILED.")
        sys.exit(1)


if __name__ == "__main__":
    main()

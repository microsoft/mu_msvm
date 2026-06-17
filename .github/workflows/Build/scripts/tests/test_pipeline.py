"""
Pytest tests for the Build pipeline scripts.

Run locally:
    pytest .github/workflows/Build/scripts/tests/test_pipeline.py
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

_TESTS_DIR = Path(__file__).resolve().parent
_SCRIPTS_DIR = _TESTS_DIR.parent
_BUILD_PIPELINE_DIR = _SCRIPTS_DIR.parent
_DATA_DIR = _BUILD_PIPELINE_DIR / "data"

sys.path.insert(0, str(_SCRIPTS_DIR))

from ci_common import (  # noqa: E402
    VALID_ARCHS,
    VALID_TARGETS,
    VALID_TOOLCHAINS,
    build_dir_for,
    load_variants,
)
import stage_artifacts  # noqa: E402


class TestVariantsYaml:
    """matrix.yml is the single source of truth for build variants."""

    def test_variants_file_exists(self) -> None:
        assert (_DATA_DIR / "matrix.yml").exists()

    def test_variants_load(self) -> None:
        variants = load_variants()
        assert len(variants) >= 1

    def test_variants_use_valid_values(self) -> None:
        for v in load_variants():
            assert v["arch"] in VALID_ARCHS
            assert v["target"] in VALID_TARGETS
            assert v["tools"] in VALID_TOOLCHAINS

    def test_no_aarch64_vs2022(self) -> None:
        """VS2022 cannot produce AARCH64 EDK2 builds in our toolchain config."""
        for v in load_variants():
            assert not (v["arch"] == "AARCH64" and v["tools"] == "VS2022"), \
                f"AARCH64 + VS2022 is not a supported variant: {v}"


class TestBuildDirFor:
    """build_dir_for() naming must match Stuart's output layout."""

    def test_x64_debug_vs2022(self) -> None:
        assert build_dir_for("X64", "DEBUG", "VS2022") == \
            Path("Build") / "MsvmX64" / "DEBUG_VS2022"

    def test_aarch64_release_clangpdb(self) -> None:
        assert build_dir_for("AARCH64", "RELEASE", "CLANGPDB") == \
            Path("Build") / "MsvmAARCH64" / "RELEASE_CLANGPDB"


class TestGenerateMatrix:
    """generate_matrix.py emits JSON readable by GitHub Actions fromJson()."""

    def test_emits_one_line_matrix_assignment(self) -> None:
        result = subprocess.run(
            [sys.executable, str(_SCRIPTS_DIR / "generate_matrix.py")],
            capture_output=True, text=True, check=True,
        )
        out = result.stdout.strip()
        assert out.startswith("matrix=")
        data = json.loads(out[len("matrix="):])
        assert "include" in data
        assert len(data["include"]) == len(load_variants())
        for row in data["include"]:
            assert set(row.keys()) == {"arch", "target", "tools"}


class TestCliRejectsBadArgs:
    """Each entrypoint script rejects missing required args (exit != 0)."""

    @pytest.mark.parametrize("script", [
        "invoke_build.py",
        "stage_artifacts.py",
        "install_vs_components.py",
    ])
    def test_no_args_fails(self, script: str) -> None:
        result = subprocess.run(
            [sys.executable, str(_SCRIPTS_DIR / script)],
            capture_output=True, text=True,
        )
        assert result.returncode != 0


class TestStageArtifacts:
    """Staged logs retain enough path context to avoid basename collisions."""

    def test_preserves_relative_log_paths(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        repo_root = tmp_path / "repo"
        build_dir = repo_root / "Build" / "MsvmX64" / "DEBUG_VS2022"
        module_a = build_dir / "ModuleA"
        module_b = build_dir / "ModuleB"
        module_a.mkdir(parents=True)
        module_b.mkdir(parents=True)
        (module_a / "BuildLog.txt").write_text("module a")
        (module_b / "BuildLog.txt").write_text("module b")

        monkeypatch.setattr(stage_artifacts, "REPO_ROOT", repo_root)
        out_dir = tmp_path / "staged"

        stage_artifacts.stage("X64", "DEBUG", "VS2022", out_dir)

        assert (out_dir / "Build Logs" / "ModuleA" / "BuildLog.txt").read_text() == "module a"
        assert (out_dir / "Build Logs" / "ModuleB" / "BuildLog.txt").read_text() == "module b"

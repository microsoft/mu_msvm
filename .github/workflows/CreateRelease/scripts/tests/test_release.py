"""
Pytest tests for the CreateRelease pipeline scripts.
"""

from __future__ import annotations

import subprocess
import sys
import tarfile
from pathlib import Path

import pytest

_TESTS_DIR = Path(__file__).resolve().parent
_SCRIPTS_DIR = _TESTS_DIR.parent
_RELEASE_PIPELINE_DIR = _SCRIPTS_DIR.parent

sys.path.insert(0, str(_SCRIPTS_DIR))

from compute_version import _next_patch  # noqa: E402
from release_common import load_release_config  # noqa: E402


class TestReleaseYaml:
    def test_loads(self) -> None:
        cfg = load_release_config()
        assert "baseVersion" in cfg
        assert "tagPrefix" in cfg


class TestNextPatch:
    def test_no_existing_tags_returns_zero(self) -> None:
        assert _next_patch([], "v26.0.") == 0

    def test_picks_max_plus_one(self) -> None:
        assert _next_patch(["v26.0.0", "v26.0.5", "v26.0.2"], "v26.0.") == 6

    def test_ignores_other_prefixes(self) -> None:
        assert _next_patch(["v25.9.99", "v26.0.0"], "v26.0.") == 1

    def test_ignores_non_numeric_suffix(self) -> None:
        assert _next_patch(["v26.0.0", "v26.0.beta"], "v26.0.") == 1


class TestPackageArtifacts:
    def test_packs_each_subdir(self, tmp_path: Path) -> None:
        artifacts = tmp_path / "artifacts"
        release = tmp_path / "release"
        for name in ("DEBUG-X64", "RELEASE-X64"):
            d = artifacts / name
            d.mkdir(parents=True)
            (d / "MSVM.fd").write_bytes(b"\x00" * 1024)
            (d / "PDB").mkdir()
            (d / "PDB" / "x.pdb").write_text("pdb")

        result = subprocess.run(
            [sys.executable, str(_SCRIPTS_DIR / "package_artifacts.py"),
             "--artifacts-dir", str(artifacts),
             "--release-dir", str(release)],
            capture_output=True, text=True,
        )
        assert result.returncode == 0, result.stderr

        tarballs = sorted(release.glob("*.tar.gz"))
        assert [t.name for t in tarballs] == ["DEBUG-X64.tar.gz", "RELEASE-X64.tar.gz"]

        with tarfile.open(release / "DEBUG-X64.tar.gz") as tf:
            names = sorted(m.name for m in tf.getmembers())
        assert "MSVM.fd" in names
        assert any(n.startswith("PDB") for n in names)

    def test_empty_artifacts_dir_fails(self, tmp_path: Path) -> None:
        artifacts = tmp_path / "artifacts"
        artifacts.mkdir()
        result = subprocess.run(
            [sys.executable, str(_SCRIPTS_DIR / "package_artifacts.py"),
             "--artifacts-dir", str(artifacts),
             "--release-dir", str(tmp_path / "release")],
            capture_output=True, text=True,
        )
        assert result.returncode != 0


class TestComputeVersionDryRun:
    def test_dry_run_emits_tag_zero(self) -> None:
        result = subprocess.run(
            [sys.executable, str(_SCRIPTS_DIR / "compute_version.py"),
             "--repo", "fake/repo", "--dry-run"],
            capture_output=True, text=True,
        )
        assert result.returncode == 0, result.stderr
        cfg = load_release_config()
        expected = f"tag={cfg['tagPrefix']}{cfg['baseVersion']}.0"
        assert expected in result.stdout

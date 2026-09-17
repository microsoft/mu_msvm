"""
Pytest tests for get_vpack_name.py.

These tests lock the case-sensitive vpack naming rules so they cannot
silently drift from the YAML's inline ob_createvpack_packagename composition.

Run locally:
    pytest .azuredevops/pipelines/OneBranch/scripts/tests/test_get_vpack_name.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).parent.parent))

from get_vpack_name import get_vpack_name  # noqa: E402


class TestProductionVS2022:
    """Production rows: Official pipeline, VS2022, no salt — no .Test suffix."""

    def test_official_x64_debug_no_test_suffix(self) -> None:
        assert get_vpack_name("MsvmPkgX64", "Official", "X64", "DEBUG") == "MsvmPkgX64.mscoreuefi.X64.DEBUG"

    def test_official_x64_release_no_test_suffix(self) -> None:
        assert get_vpack_name("MsvmPkgX64", "Official", "X64", "RELEASE") == "MsvmPkgX64.mscoreuefi.X64.RELEASE"


class TestPRPipeline:
    """PR pipeline always produces .Test names (with and without salt)."""

    def test_pr_no_salt_has_pr_prefix_and_test(self) -> None:
        assert (
            get_vpack_name("MsvmPkgX64", "PR", "X64", "RELEASE")
            == "MsvmPkgX64.mscoreuefi.PR.X64.RELEASE.Test"
        )

    def test_pr_with_salt_exactly_one_test(self) -> None:
        assert (
            get_vpack_name("MsvmPkgX64", "PR", "X64", "DEBUG", "XDCV")
            == "MsvmPkgX64.mscoreuefi.PR.X64.DEBUG.XDCV.Test"
        )


class TestOfficialSaltedVariants:
    """Official pipeline + salt -> .Test (non-shippable side-by-side variants)."""

    def test_official_salt_xdcv(self) -> None:
        assert (
            get_vpack_name("MsvmPkgX64", "Official", "X64", "DEBUG", "XDCV")
            == "MsvmPkgX64.mscoreuefi.X64.DEBUG.XDCV.Test"
        )

    def test_official_salt_arcv_aarch64(self) -> None:
        assert (
            get_vpack_name("MsvmPkgX64", "Official", "AARCH64", "RELEASE", "ARCV")
            == "MsvmPkgX64.mscoreuefi.AARCH64.RELEASE.ARCV.Test"
        )

    def test_official_salt_xrg_linux_gcc(self) -> None:
        assert (
            get_vpack_name("MsvmPkgX64", "Official", "X64", "RELEASE", "XRG")
            == "MsvmPkgX64.mscoreuefi.X64.RELEASE.XRG.Test"
        )

    def test_official_salt_adcw_aarch64_debug(self) -> None:
        """AARCH64 ClangWindows DEBUG gets salt ADCW."""
        assert (
            get_vpack_name("MsvmPkgX64", "Official", "AARCH64", "DEBUG", "ADCW")
            == "MsvmPkgX64.mscoreuefi.AARCH64.DEBUG.ADCW.Test"
        )

    def test_official_salt_arcw_aarch64_release(self) -> None:
        """AARCH64 ClangWindows RELEASE gets salt ARCW."""
        assert (
            get_vpack_name("MsvmPkgX64", "Official", "AARCH64", "RELEASE", "ARCW")
            == "MsvmPkgX64.mscoreuefi.AARCH64.RELEASE.ARCW.Test"
        )


class TestCaseSensitivity:
    """Arch, target, salt, and repo name are passed through as-is."""

    def test_preserves_aarch64_casing(self) -> None:
        assert get_vpack_name("MsvmPkgX64", "Official", "AARCH64", "RELEASE") == "MsvmPkgX64.mscoreuefi.AARCH64.RELEASE"

    def test_preserves_repo_name_casing(self) -> None:
        assert get_vpack_name("WeirdCaseRepo", "Official", "X64", "DEBUG") == "WeirdCaseRepo.mscoreuefi.X64.DEBUG"


class TestInvariants:
    @pytest.mark.parametrize(
        "pipeline,salt",
        [
            ("Official", ""),
            ("Official", "XDCV"),
            ("PR", ""),
            ("PR", "XDCV"),
        ],
    )
    def test_test_suffix_appears_at_most_once(self, pipeline: str, salt: str) -> None:
        name = get_vpack_name("R", pipeline, "X64", "DEBUG", salt)  # type: ignore[arg-type]
        assert name.count(".Test") <= 1, f"Multiple .Test in: {name!r}"

    def test_pr_prefix_iff_pr_pipeline(self) -> None:
        assert ".mscoreuefi.PR." in get_vpack_name("R", "PR", "X64", "DEBUG")
        assert ".mscoreuefi.PR." not in get_vpack_name("R", "Official", "X64", "DEBUG")

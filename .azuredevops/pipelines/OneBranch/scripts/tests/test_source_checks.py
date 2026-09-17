"""Verify source-check orchestration and the enforced pipeline contract."""

from __future__ import annotations

import importlib.util
import subprocess
import sys
from pathlib import Path

import pytest
import yaml

sys.path.insert(0, str(Path(__file__).parent.parent))

import source_checks


_ROOT = Path(__file__).resolve().parents[5]
_ONEBRANCH = _ROOT / ".azuredevops/pipelines/OneBranch"
_CHECK_COMMAND = [
    "stuart_ci_build", "-c", ".pytool/CISettings.py",
    "-p", "MsvmPkg", "-t", "NO-TARGET", "-a", "X64,AARCH64",
    "--disable-all", "GuidCheck=run", "LineEndingCheck=run", "UncrustifyCheck=run",
]


@pytest.mark.parametrize("setup", [False, True])
def test_commands(monkeypatch: pytest.MonkeyPatch, setup: bool) -> None:
    commands: list[list[str]] = []
    directories: list[Path] = []
    monkeypatch.setattr(sys, "argv", ["source_checks.py"] + (["--setup"] if setup else []))
    monkeypatch.setattr(source_checks, "run", lambda label, command: commands.append(command))
    monkeypatch.setattr(source_checks.os, "chdir", lambda path: directories.append(path))
    source_checks.main()
    expected = []
    if setup:
        expected.extend([
            [sys.executable, "-m", "pip", "install", "-r", "pip-requirements.txt"],
            ["git", "-c", "core.autocrlf=false", "submodule", "update", "--init"],
        ])
    expected.extend([["stuart_update", "-c", ".pytool/CISettings.py"], _CHECK_COMMAND])
    assert commands == expected
    assert directories == [_ROOT]


@pytest.mark.parametrize("failed_step", range(4))
def test_failure_stops_and_propagates(monkeypatch: pytest.MonkeyPatch, failed_step: int) -> None:
    commands: list[list[str]] = []
    monkeypatch.setattr(sys, "argv", ["source_checks.py", "--setup"])
    monkeypatch.setattr(source_checks.os, "chdir", lambda path: None)

    def fail_selected_step(label: str, command: list[str]) -> None:
        commands.append(command)
        if len(commands) == failed_step + 1:
            raise subprocess.CalledProcessError(7, command)

    monkeypatch.setattr(source_checks, "run", fail_selected_step)
    with pytest.raises(SystemExit) as result:
        source_checks.main()
    assert result.value.code == 7
    assert len(commands) == failed_step + 1


def test_pipeline_contract() -> None:
    common = yaml.safe_load((_ONEBRANCH / "common.yaml").read_text())
    templates = common["extends"]["parameters"]["stages"][0]["jobs"]
    assert templates[0] == {
        "template": ".azuredevops/pipelines/OneBranch/source-checks.yml@self",
    }
    builds = yaml.safe_load((_ONEBRANCH / "jobs.yml").read_text())
    build_job = next(iter(builds["jobs"][0].values()))[0]
    assert build_job["dependsOn"] == "SourceChecks"
    assert build_job.get("condition", "succeeded()") == "succeeded()"
    job = yaml.safe_load((_ONEBRANCH / "source-checks.yml").read_text())["jobs"][0]
    assert job["job"] == "SourceChecks"
    assert not job.get("continueOnError", False)
    assert job["variables"]["GIT_CONFIG_COUNT"] == "2"
    assert job["variables"]["GIT_CONFIG_KEY_0"] == "core.autocrlf"
    assert job["variables"]["GIT_CONFIG_VALUE_0"] == "false"
    assert job["variables"]["GIT_CONFIG_KEY_1"] == "core.longpaths"
    assert job["variables"]["GIT_CONFIG_VALUE_1"] == "true"
    assert job["variables"]["ob_createvpack_enabled"] is False
    assert job["variables"]["ob_nugetPublishing_enabled"] is False
    steps = job["steps"]
    check = next(step for step in steps if step.get("script", "").endswith("source_checks.py --setup"))
    assert not check.get("continueOnError", False)
    results = next(step for step in steps if step.get("task") == "PublishTestResults@2")
    assert results["condition"] == "succeededOrFailed()"
    assert results["inputs"]["testResultsFiles"] == "Build/TestSuites.xml"
    assert results["inputs"]["failTaskOnFailedTests"] is True
    assert not results.get("continueOnError", False)
    staging = next(step for step in steps if step.get("task") == "CopyFiles@2")
    assert staging["condition"] == "succeededOrFailed()"
    assert staging["continueOnError"] is True
    assert "SETUPLOG.*" in staging["inputs"]["Contents"].splitlines()
    assert steps[-1]["script"].endswith("set_repo_auth.py unset")
    assert steps[-1]["condition"] == "always()"


def test_no_checks_are_audit_only() -> None:
    policy = yaml.safe_load((_ROOT / "MsvmPkg/MsvmPkg.ci.yaml").read_text())
    assert policy["UncrustifyCheck"]["AuditOnly"] is False
    assert not policy["GuidCheck"].get("AuditOnly", False)
    assert not policy["LineEndingCheck"].get("AuditOnly", False)


def test_settings_use_upstream_workspace_relative_package_paths(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.chdir(_ONEBRANCH)
    spec = importlib.util.spec_from_file_location("msvm_ci_settings", _ROOT / ".pytool/CISettings.py")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    settings = module.Settings()
    assert Path(settings.GetWorkspaceRoot()) == _ROOT
    submodules = ("MU_BASECORE", "Common/MU", "Feature/DEBUGGER", "Common/PATINA_EDK2")
    assert settings.GetPackagesPath() == (".", *submodules)
    assert isinstance(settings, module.SetupSettingsManager)
    required = settings.GetRequiredSubmodules()
    assert tuple(submodule.path for submodule in required) == submodules
    assert all(submodule.recursive for submodule in required)
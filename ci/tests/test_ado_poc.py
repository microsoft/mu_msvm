"""Lock ADO adapter contracts without executing hosted tasks."""

import importlib
import unittest
from pathlib import Path
from typing import Any, cast

ROOT = Path(__file__).resolve().parents[2]


def load_yaml(path: str) -> dict[str, Any]:
    """Load an ADO template for contract assertions; never expand or execute it."""
    yaml = importlib.import_module("yaml")
    return cast(dict[str, Any], yaml.safe_load((ROOT / path).read_text()))


class AdoPocTests(unittest.TestCase):
    """Preserve provider-specific environment preparation."""

    def test_build_job_labels_are_resolved_at_template_expansion(self) -> None:
        job = load_yaml(".azuredevops/templates/build.yml")["jobs"][1]["${{ each host in parameters.hosts }}"][0]
        self.assertEqual(job["displayName"], "Firmware (${{ host }})")
        self.assertNotIn("$(", job["displayName"])
        self.assertEqual(job["strategy"]["matrix"],
                         "$[ dependencies.BuildMatrix.outputs['${{ host }}.matrix'] ]")

    def test_source_check_diagnostics_redact_git_credentials(self) -> None:
        source = load_yaml(".azuredevops/templates/source-checks.yml")["jobs"][0]
        step = next(step for step in source["steps"] if step.get("displayName") == "Run Source Checks")
        self.assertEqual(step["script"], "python ci/scripts/source_checks.py --verbose")
        for variable in ("GIT_TRACE", "GIT_TRACE_CURL", "GIT_TRACE_CURL_NO_DATA", "GIT_TRACE_REDACT"):
            self.assertEqual(step["env"][variable], "1")

    def test_windows_jobs_use_image_python_without_downloading(self) -> None:
        """All Windows callers select the image interpreter; Linux keeps its venv."""
        reference = {"template": "/.azuredevops/templates/windows-python.yml@self"}
        template = load_yaml(".azuredevops/templates/build.yml")
        self.assertEqual(template["jobs"][0]["steps"][0], reference)
        source = load_yaml(".azuredevops/templates/source-checks.yml")["jobs"][0]
        self.assertEqual(source["steps"][0], reference)
        job = template["jobs"][1]["${{ each host in parameters.hosts }}"][0]
        windows = next(branch["${{ if eq(host, 'windows') }}"] for branch in job["steps"]
                       if "${{ if eq(host, 'windows') }}" in branch)
        linux = next(branch["${{ if eq(host, 'linux') }}"] for branch in job["steps"]
                     if "${{ if eq(host, 'linux') }}" in branch)
        self.assertEqual(windows[0], reference)
        self.assertNotIn(reference, linux)
        self.assertIn('python3 -m venv "$BUILD_VENV"', next(step["bash"] for step in linux if "bash" in step))
        script = load_yaml(".azuredevops/templates/windows-python.yml")["steps"][0]["pwsh"]
        self.assertIn('py -3.12 -c "import sys; print(sys.executable)"', script)
        self.assertIn("if ($LASTEXITCODE -ne 0 -or -not $python)", script)
        self.assertIn("$python = $python.Trim()", script)
        self.assertIn("Test-Path -LiteralPath $python -PathType Leaf", script)
        self.assertIn("$pythonHome = Split-Path -Parent $python", script)
        self.assertNotIn("C:\\Program Files\\Python312", script)
        self.assertIn("sys.version_info[:2] == (3, 12)", script)
        self.assertIn('task.prependpath]$pythonHome\\Scripts', script)
        self.assertIn('task.prependpath]$pythonHome"', script)
        self.assertIn("-m pip --version", script)
        self.assertNotIn("install", script)
        for path in (ROOT / ".azuredevops/templates").glob("*.yml"):
            self.assertNotIn("UsePythonVersion@", path.read_text(), str(path))

    def test_repository_auth_wraps_source_checks_and_both_build_hosts(self) -> None:
        reference = "/.azuredevops/templates/repo-auth.yml@self"
        auth = load_yaml(".azuredevops/templates/repo-auth.yml")["steps"][0]
        self.assertEqual(auth["${{ if eq(parameters.mode, 'unset') }}"]["condition"], "always()")
        self.assertEqual(auth["${{ if eq(parameters.mode, 'set') }}"]["env"],
                         {"SYSTEM_ACCESSTOKEN": "$(System.AccessToken)"})
        self.assertIn("ci/scripts/repo_auth.py", auth["script"])
        source = load_yaml(".azuredevops/templates/source-checks.yml")["jobs"][0]["steps"]
        job = load_yaml(".azuredevops/templates/build.yml")["jobs"][1]["${{ each host in parameters.hosts }}"][0]
        groups = [(source, "ci/scripts/source_checks.py")]
        for host in ("windows", "linux"):
            key = "${{ if eq(host, '" + host + "') }}"
            steps = next(branch[key] for branch in job["steps"] if key in branch)
            groups.append((steps, "ci/scripts/build.py"))
        for steps, runner in groups:
            setup = next(i for i, step in enumerate(steps)
                         if step.get("template") == reference and step["parameters"]["mode"] == "set")
            cleanup = next(i for i, step in enumerate(steps)
                           if step.get("template") == reference and step["parameters"]["mode"] == "unset")
            invocation = next(i for i, step in enumerate(steps)
                              if runner in step.get("script", step.get("pwsh", step.get("bash", ""))))
            self.assertLess(setup, invocation)
            self.assertLess(invocation, cleanup)
            self.assertNotIn("GIT_CONFIG_VALUE_2", steps[invocation].get("env", {}))
            self.assertEqual(steps[setup]["parameters"].get("python", "python"),
                             steps[cleanup]["parameters"].get("python", "python"))


if __name__ == "__main__":
    unittest.main()

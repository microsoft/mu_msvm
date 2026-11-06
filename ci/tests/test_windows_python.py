"""Exercise the image Python version policy and real Windows bootstrap."""

import os
import re
import shutil
import subprocess
import sys
import unittest

from ci.tests.test_ado_poc import load_yaml


class WindowsPythonTests(unittest.TestCase):
    def test_runtime_version_gate_accepts_only_supported_versions(self) -> None:
        script = load_yaml(".azuredevops/templates/windows-python.yml")["steps"][0]["pwsh"]
        self.assertIn("Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1", script)
        match = re.search(r'& \$python -c "([^"]+)"', script)
        self.assertIsNotNone(match)
        assert match is not None
        for version, expected in (((3, 11), 1), ((3, 12), 0), ((3, 13), 0), ((3, 14), 0), ((3, 15), 1)):
            with self.subTest(version=version):
                result = subprocess.run(
                    [sys.executable, "-c", f"import sys; sys.version_info = {version!r}; {match[1]}"],
                    text=True, capture_output=True,
                )
                self.assertEqual(result.returncode, expected, result.stdout + result.stderr)

    @unittest.skipUnless(os.name == "nt" and shutil.which("powershell"), "Requires Windows PowerShell")
    def test_real_default_python_is_resolved_and_added_to_path(self) -> None:
        powershell = shutil.which("powershell")
        assert powershell is not None
        script = load_yaml(".azuredevops/templates/windows-python.yml")["steps"][0]["pwsh"]
        result = subprocess.run(
            [powershell, "-NoProfile", "-NonInteractive", "-Command", script],
            text=True, capture_output=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Image Python command:", result.stdout)
        self.assertIn("pip ", result.stdout)
        self.assertEqual(result.stdout.count("##vso[task.prependpath]"), 2)


if __name__ == "__main__":
    unittest.main()

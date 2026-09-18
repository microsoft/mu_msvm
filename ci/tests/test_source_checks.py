import importlib.util
import sys
import unittest
from pathlib import Path
from unittest.mock import call, patch

SCRIPT_PATH = Path(__file__).parents[1] / "scripts" / "source_checks.py"
SPEC = importlib.util.spec_from_file_location("source_checks", SCRIPT_PATH)
assert SPEC is not None and SPEC.loader is not None
source_checks = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(source_checks)


class SourceCheckTests(unittest.TestCase):
    @patch.object(source_checks.subprocess, "run")
    def test_source_checks_use_native_stuart_commands(self, run_mock):
        source_checks.main([])

        expected_commands = [
            [sys.executable, "-m", "pip", "install", "-r", "pip-requirements.txt"],
            ["stuart_setup", "-c", ".pytool/CISettings.py"],
            ["stuart_update", "-c", ".pytool/CISettings.py"],
            [
                "stuart_ci_build",
                "-c",
                ".pytool/CISettings.py",
                "--disable-all",
                "GuidCheck=run",
                "LineEndingCheck=run",
                "UncrustifyCheck=run",
            ],
        ]
        self.assertEqual(
            run_mock.call_args_list,
            [call(command, cwd=source_checks.REPO_ROOT, check=True) for command in expected_commands],
        )


if __name__ == "__main__":
    unittest.main()

"""Verify the shared source-check sequence without invoking pip or Stuart."""

import sys
import subprocess
import unittest
from unittest.mock import MagicMock, call, patch

from ci.scripts import source_checks


class SourceCheckTests(unittest.TestCase):
    """Ensure provider callers and local developers use the native lifecycle."""

    @patch("ci.scripts.source_checks.subprocess.run")
    def test_source_checks_use_native_stuart_commands(self, run_mock: MagicMock) -> None:
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

    @patch("ci.scripts.source_checks.subprocess.run")
    def test_verbose_only_enables_stuart_setup_logging(self, run_mock: MagicMock) -> None:
        source_checks.main(["--verbose"])
        commands = [invocation.args[0] for invocation in run_mock.call_args_list]
        self.assertEqual(len(commands), 4)
        self.assertEqual(commands[1], ["stuart_setup", "-c", ".pytool/CISettings.py", "--verbose"])
        for index in (0, 2, 3):
            self.assertNotIn("--verbose", commands[index])

    @patch("ci.scripts.source_checks.subprocess.run")
    def test_verbose_setup_failure_stops_checks(self, run_mock: MagicMock) -> None:
        run_mock.side_effect = [None, subprocess.CalledProcessError(1, "stuart_setup")]
        with self.assertRaises(subprocess.CalledProcessError):
            source_checks.main(["--verbose"])
        self.assertEqual(run_mock.call_count, 2)


if __name__ == "__main__":
    unittest.main()

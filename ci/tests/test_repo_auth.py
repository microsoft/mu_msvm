"""Test CI Git authentication without touching the developer's Git configuration."""

import io
import os
import subprocess
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from contextlib import redirect_stderr, redirect_stdout
from unittest.mock import MagicMock, patch

from ci.scripts import repo_auth

URL = "https://dev.azure.com/example/project/_git/basecore"


class RepoAuthTests(unittest.TestCase):
    def test_real_git_reads_header_in_separate_process_and_cleanup_removes_it(self) -> None:
        with TemporaryDirectory() as directory:
            config = Path(directory) / "gitconfig"
            environment = {key: value for key, value in os.environ.items() if not key.startswith("GIT_CONFIG")}
            environment.update({
                "GIT_CONFIG_GLOBAL": str(config),
                "GIT_CONFIG_NOSYSTEM": "1",
                "SYSTEM_ACCESSTOKEN": "test-token",
            })
            with patch.dict(os.environ, environment, clear=True), redirect_stdout(io.StringIO()):
                self.assertEqual(repo_auth.main(["set", "--url", URL]), 0)
                try:
                    result = subprocess.run(
                        ["git", "-C", directory, "config", "--get-urlmatch", "http.extraheader", URL],
                        capture_output=True, text=True, check=True,
                    )
                    self.assertEqual(result.stdout.strip(), "AUTHORIZATION: bearer test-token")
                finally:
                    self.assertEqual(repo_auth.main(["unset", "--url", URL]), 0)
                self.assertEqual(repo_auth.main(["unset", "--url", URL]), 0)
                result = subprocess.run(
                    ["git", "-C", directory, "config", "--get-urlmatch", "http.extraheader", URL],
                    capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, 1)
                self.assertNotIn("test-token", config.read_text())

    @patch("ci.scripts.repo_auth.subprocess.run")
    def test_set_uses_global_url_scoped_header_without_logging_token(self, run: MagicMock) -> None:
        run.return_value = subprocess.CompletedProcess([], 0)
        output = io.StringIO()
        with patch.dict(os.environ, {"SYSTEM_ACCESSTOKEN": "test-token", "GIT_TRACE": "1",
                                     "GIT_TRACE2": "1", "GIT_CURL_VERBOSE": "1"}), redirect_stdout(output):
            self.assertEqual(repo_auth.main(["set", "--url", URL]), 0)
        self.assertEqual(run.call_args.args[0], [
            "git", "config", "--global", "--replace-all", f"http.{URL}.extraheader",
            "AUTHORIZATION: bearer test-token",
        ])
        environment = run.call_args.kwargs["env"]
        self.assertFalse(any(key.startswith("GIT_TRACE") for key in environment))
        self.assertNotIn("GIT_CURL_VERBOSE", environment)
        self.assertNotIn("test-token", output.getvalue())

    @patch("ci.scripts.repo_auth.subprocess.run")
    def test_cleanup_is_idempotent_and_needs_no_token(self, run: MagicMock) -> None:
        for code in (0, 5):
            run.return_value = subprocess.CompletedProcess([], code)
            with patch.dict(os.environ, {}, clear=True), redirect_stdout(io.StringIO()):
                self.assertEqual(repo_auth.main(["unset", "--url", URL]), 0)
            self.assertEqual(run.call_args.args[0], [
                "git", "config", "--global", "--unset-all", f"http.{URL}.extraheader",
            ])

    @patch("ci.scripts.repo_auth.subprocess.run")
    def test_git_errors_are_not_suppressed_or_echoed(self, run: MagicMock) -> None:
        run.return_value = subprocess.CompletedProcess([], 3, stderr=b"test-token")
        for mode in ("set", "unset"):
            error = io.StringIO()
            with patch.dict(os.environ, {"SYSTEM_ACCESSTOKEN": "test-token"}), redirect_stderr(error):
                self.assertEqual(repo_auth.main([mode, "--url", URL]), 3)
            self.assertIn("failed (exit 3)", error.getvalue())
            self.assertNotIn("test-token", error.getvalue())

    @patch("ci.scripts.repo_auth.subprocess.run")
    def test_missing_token_and_invalid_url_fail_before_git(self, run: MagicMock) -> None:
        for token in ("", "$(System.AccessToken)", "bad\ntoken"):
            with patch.dict(os.environ, {"SYSTEM_ACCESSTOKEN": token}), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    repo_auth.main(["set", "--url", URL])
        for url in ("http://example.com/repo", "https://user:pass@example.com/repo"):
            with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                repo_auth.main(["unset", "--url", url])
        run.assert_not_called()


if __name__ == "__main__":
    unittest.main()

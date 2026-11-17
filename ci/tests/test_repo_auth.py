"""Exercise ADO template Git authentication using an isolated global config."""

import os
import subprocess
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from ci.tests.template_helpers import POWERSHELL, run_template

TEMPLATE = ".azuredevops/templates/repo-auth.yml"
URL = "https://dev.azure.com/microsoft/OS/_git/hyperv.uefi.mu_basecore"


@unittest.skipUnless(POWERSHELL, "Requires Windows PowerShell")
class RepoAuthTests(unittest.TestCase):
    def test_real_git_header_cleanup_and_trace_redaction(self) -> None:
        with TemporaryDirectory() as temporary:
            config = Path(temporary) / "gitconfig"
            environment = {
                "GIT_CONFIG_GLOBAL": str(config), "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_COUNT": "0",
                "SYSTEM_ACCESSTOKEN": "test-token", "AUTH_MODE": "set",
                "GIT_TRACE": "1", "GIT_TRACE2": "1", "GIT_CURL_VERBOSE": "1",
            }
            for mode in ("set", "unset", "unset"):
                environment["AUTH_MODE"] = mode
                result = run_template(TEMPLATE, environment)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertNotIn("test-token", result.stdout + result.stderr)
                query = subprocess.run(
                    ["git", "-C", temporary, "config", "--get-urlmatch", "http.extraheader", URL],
                    env={**os.environ, **environment, "GIT_TRACE": "0", "GIT_TRACE2": "0"},
                    capture_output=True, text=True,
                )
                if mode == "set":
                    self.assertEqual(query.returncode, 0, query.stderr)
                    self.assertEqual(query.stdout.strip(), "AUTHORIZATION: bearer test-token")
                else:
                    self.assertEqual(query.returncode, 1)
                    self.assertNotIn("test-token", config.read_text())

    def test_invalid_tokens_and_git_failures_are_explicit_and_redacted(self) -> None:
        with TemporaryDirectory() as temporary:
            config = Path(temporary) / "gitconfig"
            environment = {"GIT_CONFIG_GLOBAL": str(config), "GIT_CONFIG_NOSYSTEM": "1", "AUTH_MODE": "set"}
            for token in ("", " ", "$(System.AccessToken)", "bad\ntoken"):
                result = run_template(TEMPLATE, {**environment, "SYSTEM_ACCESSTOKEN": token})
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("must contain the pipeline job token", result.stderr)
                self.assertFalse(config.exists())
            config.mkdir()
            for mode in ("set", "unset"):
                result = run_template(TEMPLATE, {**environment, "AUTH_MODE": mode, "SYSTEM_ACCESSTOKEN": "test-token"})
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("failed (exit", result.stderr)
                self.assertNotIn("test-token", result.stdout + result.stderr)

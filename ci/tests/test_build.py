import importlib.util
import io
import os
import subprocess
import sys
import sysconfig
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch

SCRIPT_PATH = Path(__file__).parents[1] / "scripts" / "build.py"
SPEC = importlib.util.spec_from_file_location("build", SCRIPT_PATH)
assert SPEC is not None and SPEC.loader is not None
build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(build)


class BuildTests(unittest.TestCase):
    ARGS = ["--arch", "X64", "--target", "DEBUG", "--tool-chain", "CLANGPDB", "--core", "legacy"]

    def setUp(self):
        self.output = redirect_stdout(io.StringIO())
        self.output.__enter__()
        self.addCleanup(self.output.__exit__, None, None, None)

    @patch.object(build.subprocess, "run")
    def test_platform_preparation_precedes_build(self, run_mock):
        self.assertEqual(build.main(self.ARGS), 0)
        calls = run_mock.call_args_list
        self.assertEqual(len(calls), 4)
        self.assertEqual(calls[0].args[0], [sys.executable, "-m", "pip", "install", "-r", "pip-requirements.txt"])
        suffix = ".exe" if os.name == "nt" else ""
        for invocation, tool in zip(calls[1:], ("stuart_setup", "stuart_update", "stuart_build")):
            self.assertEqual(invocation.args[0][:7], [
                str(Path(sysconfig.get_path("scripts")) / (tool + suffix)),
                "-c", "MsvmPkg/PlatformBuild.py", "BUILD_ARCH=X64", "TARGET=DEBUG",
                "TOOL_CHAIN_TAG=CLANGPDB", "BLD_*_USE_LEGACY_C_CORE=TRUE",
            ])
        for invocation in calls:
            self.assertEqual(invocation.kwargs, {"cwd": SCRIPT_PATH.resolve().parents[2], "check": True})
        self.assertIn("BUILDREPORT_TYPES=PCD DEPEX FLASH BUILD_FLAGS LIBRARY", calls[-1].args[0])
        self.assertIn("LaunchLogOnSuccess=FALSE", calls[-1].args[0])
        self.assertIn("LaunchLogOnError=FALSE", calls[-1].args[0])

    @patch.object(build.subprocess, "run")
    def test_patina_release_flavor(self, run_mock):
        build.main(["--arch", "AARCH64", "--target", "RELEASE", "--tool-chain", "CLANGPDB", "--core", "patina"])
        for invocation in run_mock.call_args_list[1:]:
            self.assertIn("BLD_*_USE_LEGACY_C_CORE=FALSE", invocation.args[0])
            self.assertIn("BUILD_ARCH=AARCH64", invocation.args[0])
            self.assertIn("TARGET=RELEASE", invocation.args[0])

    @patch.object(build.subprocess, "run")
    def test_failure_stops_remaining_commands(self, run_mock):
        for failed_step in range(4):
            with self.subTest(failed_step=failed_step):
                run_mock.reset_mock()
                run_mock.side_effect = [None] * failed_step + [subprocess.CalledProcessError(7, "command")]
                self.assertEqual(build.main(self.ARGS), 7)
                self.assertEqual(run_mock.call_count, failed_step + 1)

    @patch.object(build.subprocess, "run")
    def test_dry_run_has_no_side_effects(self, run_mock):
        self.assertEqual(build.main([*self.ARGS, "--dry-run"]), 0)
        run_mock.assert_not_called()

    @patch.object(build.subprocess, "run")
    def test_invalid_or_missing_flavor_is_rejected(self, run_mock):
        for args in ([], [*self.ARGS[:-1], "unknown"]):
            with self.subTest(args=args), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    build.main(args)
                self.assertEqual(error.exception.code, 2)
        run_mock.assert_not_called()


if __name__ == "__main__":
    unittest.main()

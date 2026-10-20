"""Verify build orchestration without installing tools or compiling firmware."""

import io
import os
import subprocess
import sys
import sysconfig
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import MagicMock, patch

from ci.scripts import build

SCRIPT_PATH = Path(__file__).parents[1] / "scripts" / "build.py"


class BuildTests(unittest.TestCase):
    """Lock command ordering, flavor propagation, and fail-fast behavior."""
    ARGS = ["--arch", "X64", "--target", "DEBUG", "--tool-chain", "CLANGPDB", "--core", "legacy"]

    def setUp(self) -> None:
        """Suppress command previews without executing external processes."""
        self.output = redirect_stdout(io.StringIO())
        self.output.__enter__()
        self.addCleanup(self.output.__exit__, None, None, None)

    @patch("ci.scripts.build.subprocess.run")
    def test_platform_preparation_precedes_build(self, run_mock: MagicMock) -> None:
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
            self.assertEqual(invocation.kwargs, {"cwd": SCRIPT_PATH.resolve().parents[2], "check": True,
                                               "env": {**os.environ, "SOURCE_ORIGIN": "unknown"}})
            if invocation is not calls[0]:
                self.assertIn("BLD_*_LEGACY_DEBUGGER=0", invocation.args[0])
        self.assertIn("BUILDREPORT_TYPES=PCD DEPEX FLASH BUILD_FLAGS LIBRARY", calls[-1].args[0])
        self.assertIn("LaunchLogOnSuccess=FALSE", calls[-1].args[0])
        self.assertIn("LaunchLogOnError=FALSE", calls[-1].args[0])

    @patch("ci.scripts.build.subprocess.run")
    def test_patina_release_flavor(self, run_mock: MagicMock) -> None:
        build.main(["--arch", "AARCH64", "--target", "RELEASE", "--tool-chain", "CLANGPDB", "--core", "patina"])
        for invocation in run_mock.call_args_list[1:]:
            self.assertIn("BLD_*_USE_LEGACY_C_CORE=FALSE", invocation.args[0])
            self.assertIn("BUILD_ARCH=AARCH64", invocation.args[0])
            self.assertIn("TARGET=RELEASE", invocation.args[0])

    @patch("ci.scripts.build.subprocess.run")
    def test_failure_stops_remaining_commands(self, run_mock: MagicMock) -> None:
        for failed_step in range(4):
            with self.subTest(failed_step=failed_step):
                run_mock.reset_mock()
                run_mock.side_effect = [None] * failed_step + [subprocess.CalledProcessError(7, "command")]
                self.assertEqual(build.main(self.ARGS), 7)
                self.assertEqual(run_mock.call_count, failed_step + 1)

    @patch("ci.scripts.build.subprocess.run")
    def test_dry_run_has_no_side_effects(self, run_mock: MagicMock) -> None:
        self.assertEqual(build.main([*self.ARGS, "--dry-run"]), 0)
        run_mock.assert_not_called()

    @patch("ci.scripts.build.subprocess.run")
    def test_debugger_passed_to_all_stuart_phases(self, run_mock: MagicMock) -> None:
        build.main(["--arch", "X64", "--target", "DEBUG", "--tool-chain", "VS2022", "--core", "legacy",
                    "--source-origin", "closed", "--legacy-debugger", "1"])
        for invocation in run_mock.call_args_list[1:]:
            self.assertIn("BLD_*_LEGACY_DEBUGGER=1", invocation.args[0])

    @patch("ci.scripts.build.subprocess.run")
    def test_debugger_rejected_for_open_unknown_gcc_and_patina(self, run_mock: MagicMock) -> None:
        for origin, tool, core in (("open", "VS2022", "legacy"), ("unknown", "VS2022", "legacy"),
                                   ("open", "CLANGPDB", "legacy"), ("unknown", "CLANGPDB", "legacy"),
                                   ("closed", "GCC", "legacy"), ("closed", "VS2022", "patina")):
            with self.subTest(origin=origin, tool=tool, core=core), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    build.main(["--arch", "X64", "--target", "DEBUG", "--tool-chain", tool, "--core", core,
                                "--host", "linux" if tool == "GCC" else "windows",
                                "--source-origin", origin, "--legacy-debugger", "1", "--dry-run"])
                self.assertEqual(error.exception.code, 2)
        run_mock.assert_not_called()

    @patch("ci.scripts.build.subprocess.run")
    def test_closed_clang_debugger_variants_are_accepted(self, run_mock: MagicMock) -> None:
        """Preserve all three Clang debugger combinations configured by ADO."""
        for host, source in (("windows", "visual_studio"), ("windows", "windows_org"),
                             ("linux", "distribution")):
            with self.subTest(host=host, source=source):
                args = [*self.ARGS, "--host", host, "--compiler-source", source,
                        "--source-origin", "closed", "--legacy-debugger", "1", "--dry-run"]
                if source == "windows_org":
                    args.extend(["--clang-bin", "internal-llvm/bin"])
                output = io.StringIO()
                with redirect_stdout(output):
                    self.assertEqual(build.main(args), 0)
                self.assertEqual(output.getvalue().count("BLD_*_LEGACY_DEBUGGER=1"), 3)
        run_mock.assert_not_called()

    @patch("ci.scripts.build.subprocess.run")
    def test_source_origin_is_explicit_and_inherited_by_all_commands(self, run_mock: MagicMock) -> None:
        for origin in ("open", "closed", "unknown"):
            with self.subTest(origin=origin), patch.dict(os.environ, {"SOURCE_ORIGIN": "inherited"}):
                run_mock.reset_mock()
                self.assertEqual(build.main([*self.ARGS, "--source-origin", origin]), 0)
                self.assertEqual(os.environ["SOURCE_ORIGIN"], "inherited")
                for invocation in run_mock.call_args_list:
                    self.assertEqual(invocation.kwargs["env"]["SOURCE_ORIGIN"], origin)

    @patch("ci.scripts.build.subprocess.run")
    def test_windows_org_cannot_fall_back_to_visual_studio(self, run_mock: MagicMock) -> None:
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
            build.main([*self.ARGS, "--host", "windows", "--compiler-source", "windows_org", "--dry-run"])
        self.assertEqual(error.exception.code, 2)
        output = io.StringIO()
        with redirect_stdout(output):
            self.assertEqual(build.main([
                *self.ARGS, "--host", "windows", "--compiler-source", "windows_org",
                "--clang-bin", "internal-llvm/bin", "--dry-run",
            ]), 0)
        self.assertIn("CLANG_BIN=", output.getvalue())
        run_mock.assert_not_called()

    @patch("ci.scripts.build.subprocess.run")
    def test_arm64_msvc_is_rejected_before_setup(self, run_mock: MagicMock) -> None:
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
            build.main(["--arch", "AARCH64", "--target", "RELEASE", "--tool-chain", "VS2022", "--core", "legacy"])
        self.assertEqual(error.exception.code, 2)
        run_mock.assert_not_called()

    @patch("ci.scripts.build.subprocess.run")
    def test_explicit_clang_bin_overrides_child_environment_only(self, run_mock: MagicMock) -> None:
        with patch.dict(os.environ, {"CLANG_BIN": "visual-studio/bin/"}), patch.object(Path, "is_file", return_value=True):
            build.main([*self.ARGS, "--clang-bin", "internal-llvm/bin"])
            self.assertEqual(os.environ["CLANG_BIN"], "visual-studio/bin/")
            for invocation in run_mock.call_args_list:
                self.assertEqual(invocation.kwargs["env"]["CLANG_BIN"], Path("internal-llvm/bin").resolve().as_posix() + "/")

    @patch("ci.scripts.build.subprocess.run")
    def test_invalid_or_missing_flavor_is_rejected(self, run_mock: MagicMock) -> None:
        for args in ([], [*self.ARGS[:-1], "unknown"]):
            with self.subTest(args=args), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    build.main(args)
                self.assertEqual(error.exception.code, 2)
        run_mock.assert_not_called()


if __name__ == "__main__":
    unittest.main()

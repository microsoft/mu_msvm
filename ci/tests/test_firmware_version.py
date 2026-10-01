"""Exercise the real firmware version plugin without compiling firmware.

Port the internal repository's override regression cases to unittest. The
Stuart VarDict is loaded dynamically, like the plugin, because neither offers
a typed API. Both the binary ABI and the firmware macro values are checked.
"""

import importlib
import importlib.util
import builtins
import json
import os
import struct
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from types import ModuleType, SimpleNamespace
from typing import Any
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


def load_plugin() -> ModuleType:
    """Load the production plugin without triggering Stuart discovery."""
    path = ROOT / "MsvmPkg/BuildPlugins/FirmwareVersionBlob/FirmwareVersionBlob.py"
    spec = importlib.util.spec_from_file_location("firmware_version_blob", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FirmwareVersionTests(unittest.TestCase):
    """Build vars win over shell vars, including explicit empty/false values."""

    def test_source_origin_is_explicit_sidecar_metadata(self) -> None:
        """Origins honor Stuart precedence and leave the v1 binary ABI unchanged."""
        module = load_plugin()
        cases: list[tuple[str | None, str | None, str]] = [
            (None, None, "unknown"), ("open", None, "open"), ("closed", None, "closed"),
            ("closed", "open", "open"), ("open", "closed", "closed"),
            ("closed", "unknown", "unknown"),
        ]
        blobs: list[bytes] = []
        for shell_origin, build_origin, expected in cases:
            with self.subTest(shell=shell_origin, build=build_origin), TemporaryDirectory() as output:
                env = importlib.import_module("edk2toolext.environment.var_dict").VarDict()
                env.SetValue("BUILD_OUTPUT_BASE", output, "test")
                if build_origin is not None:
                    env.SetValue("SOURCE_ORIGIN", build_origin, "test")
                environment = {"OFFICIAL_BUILD": "1"}
                if shell_origin is not None:
                    environment["SOURCE_ORIGIN"] = shell_origin
                plugin = module.FirmwareVersionBlob()
                builder = SimpleNamespace(env=env, GetWorkspaceRoot=lambda: str(ROOT))
                with patch.dict(os.environ, environment, clear=True), \
                        patch.object(plugin, "_GetGitCommit", return_value=("a" * 40, False)):
                    self.assertEqual(plugin.do_pre_build(builder), 0)
                metadata = json.loads((Path(output) / plugin.FW_VERSION_METADATA_SUBPATH).read_text())
                self.assertEqual(metadata["source_origin"], expected)
                self.assertEqual(metadata["schema_version"], 1)
                self.assertEqual(metadata["flags"], 2)
                blobs.append((Path(output) / plugin.FW_VERSION_BLOB_SUBPATH).read_bytes())
        self.assertTrue(all(blob == blobs[0] for blob in blobs))
        self.assertEqual(len(blobs[0]), 80)

    def test_invalid_origin_is_rejected_before_emission(self) -> None:
        module = load_plugin()
        for origin in ("", "github", "official"):
            with self.subTest(origin=origin), TemporaryDirectory() as output:
                env = importlib.import_module("edk2toolext.environment.var_dict").VarDict()
                env.SetValue("BUILD_OUTPUT_BASE", output, "test")
                env.SetValue("SOURCE_ORIGIN", origin, "test")
                plugin = module.FirmwareVersionBlob()
                builder = SimpleNamespace(env=env, GetWorkspaceRoot=lambda: str(ROOT))
                with patch.object(plugin, "_GetGitCommit", return_value=("a" * 40, False)), \
                        self.assertRaisesRegex(ValueError, "SOURCE_ORIGIN"):
                    plugin.do_pre_build(builder)
                self.assertIsNone(env.GetValue("BLD_*_MSVM_FW_FLAGS"))
                self.assertFalse((Path(output) / plugin.FW_VERSION_BLOB_SUBPATH).exists())
                self.assertFalse((Path(output) / plugin.FW_VERSION_METADATA_SUBPATH).exists())

    def test_record_limits_fail_before_emitting_macros_or_blob(self) -> None:
        """Invalid overrides must not silently truncate or diverge from PCDs."""
        module = load_plugin()
        cases = [
            (1, 0, "1" * 16, "a" * 40), (1, 0, "26.0\0bad", "a" * 40),
            (1, 0, "26.\u00e9", "a" * 40), (1, 0, "26.0", "a" * 48),
            (-1, 0, "26.0", "a" * 40), (1, 65536, "26.0", "a" * 40),
        ]
        for major, minor, base, commit in cases:
            with self.subTest(major=major, minor=minor, base=base, commit=commit), TemporaryDirectory() as output:
                env = importlib.import_module("edk2toolext.environment.var_dict").VarDict()
                env.SetValue("BUILD_OUTPUT_BASE", output, "test")
                builder = SimpleNamespace(env=env, GetWorkspaceRoot=lambda: str(ROOT))
                plugin = module.FirmwareVersionBlob()
                with patch.object(plugin, "_ReadVersionToml", return_value=(major, minor, base)), \
                        patch.object(plugin, "_GetGitCommit", return_value=(commit, False)), \
                        self.assertRaises(ValueError):
                    plugin.do_pre_build(builder)
                self.assertIsNone(env.GetValue("BLD_*_MSVM_FW_BASE_VERSION"))
                self.assertFalse((Path(output) / plugin.FW_VERSION_BLOB_SUBPATH).exists())

    def test_string_boundary_reserves_nul(self) -> None:
        module = load_plugin()
        encoded = module.FirmwareVersionBlob._EncodeRecordString("1" * 15, 16, "BASE_VERSION")
        self.assertEqual(struct.pack("16s", encoded), b"1" * 15 + b"\0")

    def test_tomli_fallback_when_stdlib_toml_is_unavailable(self) -> None:
        """Exercise fallback dispatch on this interpreter without installing tomli."""
        real_import = builtins.__import__
        toml = importlib.import_module("tomllib")

        def fallback_import(name: str, *args: Any, **kwargs: Any) -> Any:
            if name == "tomllib":
                raise ModuleNotFoundError(name)
            if name == "tomli":
                return toml
            return real_import(name, *args, **kwargs)

        with patch("builtins.__import__", side_effect=fallback_import):
            module = load_plugin()
        self.assertIs(module.tomllib, toml)

    def test_provider_official_context_reaches_both_hosts(self) -> None:
        """Official provenance comes from the pipeline, never shipping metadata."""
        yaml = importlib.import_module("yaml")
        github = yaml.safe_load((ROOT / ".github/workflows/platform-ci.yml").read_text())
        build_step = next(step for step in github["jobs"]["build"]["steps"] if step.get("uses") == "./.github/actions/build")
        self.assertEqual(build_step["env"]["OFFICIAL_BUILD"],
                         "${{ (github.event_name == 'push' && github.ref == 'refs/heads/main') && '1' || '0' }}")
        action = yaml.safe_load((ROOT / ".github/actions/build/action.yml").read_text())
        self.assertIn("--source-origin open", action["runs"]["steps"][-1]["run"])
        for entry, base in (("platform-ci.yml", "Official"), ("platform-pr.yml", "NonOfficial")):
            pipeline = yaml.safe_load((ROOT / ".azuredevops/pipelines" / entry).read_text())
            self.assertEqual(pipeline["extends"]["parameters"]["oneBranchBase"], f"v2/Microsoft.{base}.yml")
        platform = yaml.safe_load((ROOT / ".azuredevops/templates/platform.yml").read_text())
        stage = next(stage for stage in platform["extends"]["parameters"]["stages"] if stage["stage"] == "Build")
        self.assertEqual(stage["jobs"][0]["parameters"]["officialBuild"],
                         "${{ eq(parameters.oneBranchBase, 'v2/Microsoft.Official.yml') }}")
        template = yaml.safe_load((ROOT / ".azuredevops/templates/build.yml").read_text())
        parameter = next(item for item in template["parameters"] if item["name"] == "officialBuild")
        self.assertIs(parameter["default"], False)
        job = template["jobs"][1]["${{ each host in parameters.hosts }}"][0]
        for branch in job["steps"]:
            steps = next(iter(branch.values()))
            build_step = next(step for step in steps if "ci/scripts/build.py" in step.get("pwsh", step.get("bash", "")))
            self.assertEqual(build_step["env"]["OFFICIAL_BUILD"], "${{ iif(parameters.officialBuild, '1', '0') }}")
            command = build_step.get("pwsh", build_step.get("bash", ""))
            self.assertTrue("'--source-origin', 'closed'" in command or "--source-origin closed" in command)

    def test_build_runner_preserves_version_environment(self) -> None:
        """The orchestration layer passes overrides to Stuart unchanged."""
        from ci.scripts import build

        with patch.dict(os.environ, {"OFFICIAL_BUILD": "1", "BASE_VERSION": "99.2"}), \
                patch("ci.scripts.build.subprocess.run") as run_mock:
            self.assertEqual(build.main([
                "--arch", "X64", "--target", "DEBUG", "--tool-chain", "CLANGPDB", "--core", "legacy",
            ]), 0)
            self.assertEqual(run_mock.call_count, 4)
            for invocation in run_mock.call_args_list:
                self.assertEqual(invocation.kwargs["env"]["OFFICIAL_BUILD"], "1")
                self.assertEqual(invocation.kwargs["env"]["BASE_VERSION"], "99.2")

    def test_blob_macros_and_log_agree(self) -> None:
        module = load_plugin()
        cases: list[tuple[str | None, str | None, str | None, str | None, bool, str | None]] = [
            (None, None, None, None, False, None),
            ("1", None, "99.2", None, True, "99.2"),
            ("", None, "", None, False, None),
            ("0", None, None, None, False, None),
            (" FaLsE ", None, None, None, False, None),
            ("1", "0", "99.2", "88.1", False, "88.1"),
            ("1", "", "99.2", "", False, None),
            ("0", "1", None, "88.1", True, "88.1"),
        ]
        for dirty in (False, True):
            for shell_official, build_official, shell_base, build_base, official, release in cases:
                with self.subTest(dirty=dirty, official=official, release=release), TemporaryDirectory() as output:
                    env = importlib.import_module("edk2toolext.environment.var_dict").VarDict()
                    env.SetValue("BUILD_OUTPUT_BASE", output, "test")
                    environment = {key: value for key, value in os.environ.items()
                                   if key not in ("OFFICIAL_BUILD", "BASE_VERSION", "SOURCE_ORIGIN")}
                    for name, shell_value, build_value in (
                        ("OFFICIAL_BUILD", shell_official, build_official),
                        ("BASE_VERSION", shell_base, build_base),
                    ):
                        if shell_value is not None:
                            environment[name] = shell_value
                        if build_value is not None:
                            env.SetValue(name, build_value, "test")
                    plugin = module.FirmwareVersionBlob()
                    with (ROOT / plugin.FW_VERSION_TOML_SUBPATH).open("rb") as manifest:
                        version = module.tomllib.load(manifest)
                    expected_release = release if release is not None else str(version["release"]["version"])
                    flags = int(dirty) | (2 if official else 0)
                    commit = "a" * 40
                    builder = SimpleNamespace(env=env, GetWorkspaceRoot=lambda: str(ROOT))
                    with patch.dict(os.environ, environment, clear=True), \
                            patch.object(plugin, "_GetGitCommit", return_value=(commit, dirty)), \
                            self.assertLogs(level="INFO") as logs:
                        self.assertEqual(plugin.do_pre_build(builder), 0)
                    blob = (Path(output) / plugin.FW_VERSION_BLOB_SUBPATH).read_bytes()
                    record = struct.unpack("<IHHIHH16s48s", blob)
                    self.assertEqual(record[:6], (0x5746564D, 1, 80, flags,
                                                 version["interface"]["major"], version["interface"]["minor"]))
                    self.assertEqual(record[6].rstrip(b"\0").decode("ascii"), expected_release)
                    self.assertEqual(record[7].rstrip(b"\0").decode("ascii"), commit)
                    metadata = json.loads((Path(output) / plugin.FW_VERSION_METADATA_SUBPATH).read_text())
                    self.assertEqual(metadata, {
                        "schema_version": 1, "struct_version": 1, "interface_major": record[4],
                        "interface_minor": record[5], "base_version": expected_release,
                        "git_commit": commit, "flags": flags, "source_origin": "unknown",
                    })
                    for macro, expected in (
                        ("FLAGS", flags), ("BASE_VERSION", expected_release), ("GIT_COMMIT", commit),
                        ("INTERFACE_MAJOR", record[4]), ("INTERFACE_MINOR", record[5]),
                    ):
                        self.assertEqual(env.GetValue(f"BLD_*_MSVM_FW_{macro}"), str(expected))
                    self.assertEqual(" (official)" in "\n".join(logs.output), official)
                    self.assertEqual(" (dirty)" in "\n".join(logs.output), dirty)


if __name__ == "__main__":
    unittest.main()

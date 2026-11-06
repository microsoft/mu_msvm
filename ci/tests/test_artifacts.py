"""Exercise real staging/archive payloads and provider publication gates."""

from __future__ import annotations

import io
import json
import os
import shutil
import subprocess
import tarfile
import unittest
import xml.etree.ElementTree as ET
import zipfile
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch

from ci.scripts import matrix, package_artifacts, stage_artifacts
from ci.tests.test_ado_poc import load_yaml

ROOT = Path(__file__).resolve().parents[2]
OPEN = ROOT / ".github/build-matrix.json"
CLOSED = ROOT / ".azuredevops/build-matrix.json"
COMMIT = "a" * 40


def fixture(workspace: Path, row: matrix.SelectedBuild, origin: str, official: bool) -> Path:
    build = workspace / "Build" / f"Msvm{row['arch']}" / f"{row['target']}_{row['tool_chain']}"
    files = {
        "FV/MSVM.fd": b"firmware", "MAP/Module.map": b"map",
        "PDB/Module.pdb": b"symbols", "FwVersion/FwVersionBlob.bin": b"\0" * 80,
        f"{row['arch']}/MsvmPkg/AcpiTables/AcpiTables/OUTPUT/Dsdt.aml": b"aml",
        "FwVersion/FirmwareVersion.json": json.dumps({
            "schema_version": 1, "struct_version": 1, "base_version": package_artifacts.base_version(),
            "git_commit": COMMIT, "flags": 2 if official else 0, "source_origin": origin,
        }).encode(),
        "BUILD_REPORT.TXT": b"report", "Nested/BUILD_REPORT.TXT": b"nested report",
    }
    for name, contents in files.items():
        path = build / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(contents)
    header = workspace / "MsvmPkg/Include/BiosInterface.h"
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text("header")
    (workspace / "Build/SETUPLOG.txt").write_text("setup")
    return build


class ArtifactTests(unittest.TestCase):
    def test_ado_preserves_legacy_folders_for_all_15_flavors(self) -> None:
        for row in matrix.select_builds(CLOSED):
            with self.subTest(flavor=row["id"]), TemporaryDirectory() as temporary, redirect_stdout(io.StringIO()):
                root = Path(temporary)
                workspace, output = root / "workspace", root / "output"
                fixture(workspace, row, "closed", False)
                stage_artifacts.stage_build(workspace, output, row, True, ado=True)
                suffix = f"{row['target']}_{row['arch']}"
                self.assertEqual((output / f"Firmware Binary File {suffix}/MSVM.fd").read_bytes(), b"firmware")
                self.assertTrue((output / f"Firmware Map Files {suffix}/Module.map").is_file())
                self.assertEqual((output / f"Firmware PDB Files {suffix}/Module.pdb").exists(), row["tool_chain"] != "GCC")
                label = f"{row['host'].title()} {row['arch']} {row['target']} {row['tool_chain']}"
                if row["compiler_source"] == "windows_org":
                    label += " (WinOrg)"
                self.assertTrue((output / f"Build Logs {label}/BUILD_REPORT.TXT").is_file())
                self.assertEqual(len(list((output / "logs").rglob("BUILD_REPORT.TXT"))), 2)
                legacy_bin = output / "Build" / f"Msvm{row['arch']}" / f"{row['target']}_{row['tool_chain']}" / "Bin"
                for name in ("MSVM.fd", "Dsdt.aml", "BiosInterface.h", "FirmwareVersion.json", "FwVersionBlob.bin"):
                    self.assertTrue((legacy_bin / name).is_file(), name)

    def test_ado_failure_adds_legacy_all_tree_after_partial_staging(self) -> None:
        with TemporaryDirectory() as temporary, redirect_stdout(io.StringIO()):
            root = Path(temporary)
            row = matrix.select_builds(CLOSED)[0]
            build = fixture(root / "workspace", row, "closed", False)
            (build / "MAP/Module.map").unlink()
            arguments = ["--matrix", str(CLOSED), "--build-id", row["id"], "--ado-layout",
                         "--output", str(root / "output")]
            with patch.object(Path, "cwd", return_value=root / "workspace"), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    stage_artifacts.main([*arguments, "--build-succeeded"])
                stage_artifacts.main(arguments)
            self.assertTrue((root / "output/all/MsvmX64/DEBUG_VS2022/FV/MSVM.fd").is_file())
            self.assertTrue((root / "output/Build Logs Windows X64 DEBUG VS2022/SETUPLOG.txt").is_file())

    def test_source_check_compatibility_keeps_package_diagnostics(self) -> None:
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "workspace/Build"
            (build / "MsvmPkg/Uncrustify").mkdir(parents=True)
            for name in ("CI_BUILDLOG.md", "SETUPLOG.txt", "UPDATE_LOG.txt", "TestSuites.xml",
                         "MsvmPkg/Uncrustify/output.diff"):
                (build / name).write_text(name)
            stage_artifacts.source_check_logs(root / "workspace", root / "output", True)
            for name in ("CI_BUILDLOG.md", "SETUPLOG.txt", "UPDATE_LOG.txt", "TestSuites.xml",
                         "MsvmPkg/Uncrustify/output.diff"):
                self.assertEqual((root / "output" / name).read_text(), name)
                self.assertEqual((root / "output/logs" / name).read_text(), name)

    def test_all_25_flavors_stage_expected_payloads(self) -> None:
        for coverage, origin in ((OPEN, "open"), (CLOSED, "closed")):
            for row in matrix.select_builds(coverage):
                with self.subTest(flavor=row["id"]), TemporaryDirectory() as temporary:
                    workspace = Path(temporary) / "workspace"
                    build = fixture(workspace, row, origin, False)
                    if row["tool_chain"] == "GCC":
                        (build / "PDB/Module.pdb").unlink()
                    output = Path(temporary) / "output"
                    stage_artifacts.stage_build(workspace, output, row, True)
                    firmware = output / "firmware"
                    self.assertEqual((firmware / "FV/MSVM.fd").read_bytes(), b"firmware")
                    self.assertEqual((firmware / "MAP/Module.map").read_bytes(), b"map")
                    self.assertEqual((firmware / "PDB/Module.pdb").exists(), row["tool_chain"] != "GCC")
                    self.assertEqual((firmware / "FwVersion/FirmwareVersion.json").read_bytes(),
                                     (build / "FwVersion/FirmwareVersion.json").read_bytes())
                    self.assertEqual(json.loads((firmware / "BuildInfo.json").read_text()), row)
                    self.assertEqual(len(list((output / "logs").rglob("BUILD_REPORT.TXT"))), 2)
                    self.assertTrue((output / "logs/SETUPLOG.txt").is_file())
                    package = output / "package/Bin"
                    self.assertEqual(package.exists(), row.get("shipping") == "true")
                    if package.exists():
                        self.assertEqual({file.relative_to(package).as_posix() for file in package.rglob("*") if file.is_file()},
                                         {"MSVM.fd", "Dsdt.aml", "BiosInterface.h", "PDB/Module.pdb",
                                          "FirmwareVersion.json", "FwVersionBlob.bin", "BuildInfo.json"})

    def test_success_rejects_missing_outputs_but_retains_logs(self) -> None:
        row = matrix.select_builds(CLOSED)[0]
        for missing in ("FV/MSVM.fd", "MAP/Module.map", "PDB/Module.pdb",
                        "FwVersion/FirmwareVersion.json", "FwVersion/FwVersionBlob.bin",
                        "X64/MsvmPkg/AcpiTables/AcpiTables/OUTPUT/Dsdt.aml"):
            with self.subTest(missing=missing), TemporaryDirectory() as temporary:
                workspace = Path(temporary) / "workspace"
                build = fixture(workspace, row, "closed", False)
                (build / missing).unlink()
                output = Path(temporary) / "output"
                with self.assertRaises(ValueError):
                    stage_artifacts.stage_build(workspace, output, row, True)
                self.assertTrue((output / "logs/SETUPLOG.txt").is_file())

    def test_failure_preserves_build_tree_without_releasable_firmware(self) -> None:
        row = matrix.select_builds(OPEN)[0]
        with TemporaryDirectory() as temporary:
            workspace = Path(temporary) / "workspace"
            fixture(workspace, row, "open", False)
            output = Path(temporary) / "output"
            stage_artifacts.stage_build(workspace, output, row, False)
            self.assertFalse((output / "firmware").exists())
            self.assertTrue((output / "logs/failed-build/FV/MSVM.fd").is_file())

    def test_early_failure_reports_missing_logs_and_outputs(self) -> None:
        with TemporaryDirectory() as temporary, patch("builtins.print") as log:
            stage_artifacts.stage_build(Path(temporary), Path(temporary) / "out", matrix.select_builds(OPEN)[0], False)
            self.assertEqual(log.call_count, 2)

    def test_cli_rejects_unsafe_or_reused_output_and_unknown_flavor(self) -> None:
        with TemporaryDirectory() as temporary:
            workspace = Path(temporary)
            (workspace / "used").mkdir()
            (workspace / "used/file").write_text("keep")
            for destination in (workspace, workspace / "Build/artifacts", workspace / "used"):
                with patch.object(Path, "cwd", return_value=workspace), redirect_stderr(io.StringIO()), \
                        self.assertRaises(SystemExit):
                    stage_artifacts.main(["--logs-only", "--output", str(destination)])
            with self.assertRaisesRegex(ValueError, "Build ID"):
                stage_artifacts.select_flavor(OPEN, "missing")

    def test_release_archives_preserve_names_layout_and_sidecars(self) -> None:
        rows = matrix.select_builds(OPEN)
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            for row, label in zip(rows, matrix.label_builds(rows, "github")):
                workspace = root / row["id"]
                fixture(workspace, row, "open", True)
                stage = root / "staged" / row["id"]
                stage_artifacts.stage_build(workspace, stage, row, True)
                destination = root / "downloaded" / label["artifact_name"]
                shutil.copytree(stage / "firmware", destination)
            package_artifacts.release_archives(OPEN, root / "downloaded", root / "release", COMMIT)
            self.assertEqual({file.name for file in (root / "release").iterdir()},
                             {f"{row['artifact_name']}.tar.gz" for row in matrix.label_builds(rows, "github")})
            for file in (root / "release").iterdir():
                with tarfile.open(file) as archive:
                    self.assertEqual({item.name for item in archive if item.isfile()},
                                     {"FV/MSVM.fd", "PDB/Module.pdb", "MAP/Module.map",
                                      "FwVersion/FirmwareVersion.json", "FwVersion/FwVersionBlob.bin", "BuildInfo.json"})
            shutil.rmtree(root / "downloaded" / matrix.label_builds(rows, "github")[0]["artifact_name"])
            with self.assertRaisesRegex(ValueError, "exactly match"):
                package_artifacts.release_archives(OPEN, root / "downloaded", root / "incomplete", COMMIT)
            self.assertFalse((root / "incomplete").exists())

    def test_nuget_identity_and_payload_for_all_15_flavors(self) -> None:
        for row in matrix.select_builds(CLOSED):
            for official in (True, False):
                with self.subTest(flavor=row["id"], official=official), TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    build = fixture(root / "workspace", row, "closed", official)
                    if row["tool_chain"] == "GCC":
                        (build / "PDB/Module.pdb").unlink()
                    with redirect_stdout(io.StringIO()):
                        stage_artifacts.stage_build(root / "workspace", root / "staged", row, True, ado=True)
                    output = root / "package.nuspec"
                    package_artifacts.nuget_manifest(CLOSED, row["id"], root / "staged", output,
                                                     COMMIT, "hyperv.uefi", "main" if official else "merge", official)
                    manifest = ET.parse(output)
                    prefix = "" if official else "PR."
                    salt = f".{row['package_suffix']}" if row["package_suffix"] else ""
                    suffix = ".Test" if not official or row["shipping"] != "true" else ""
                    self.assertEqual(manifest.findtext("metadata/id"),
                                     f"hyperv.uefi.mscoreuefi.{prefix}{row['arch']}.{row['target']}{salt}{suffix}")
                    legacy = f"Msvm{row['arch']}/{row['target']}_{row['tool_chain']}"
                    targets = {"", f"{legacy}/FV", f"map/{legacy}/MAP", "FwVersion"}
                    if row["tool_chain"] != "GCC":
                        targets.add(f"symbols/{legacy}/PDB")
                    self.assertEqual({file.get("target") for file in manifest.findall("files/file")},
                                     targets)
                    self.assertTrue(all(Path(file.attrib["src"]).is_absolute() for file in manifest.findall("files/file")))

    def test_rejects_missing_package_metadata_or_wrong_provenance(self) -> None:
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            row = matrix.select_builds(CLOSED)[0]
            fixture(root / "workspace", row, "closed", True)
            stage_artifacts.stage_build(root / "workspace", root / "staged", row, True)
            firmware = root / "staged/firmware"
            metadata_path = firmware / "FwVersion/FirmwareVersion.json"
            metadata = json.loads(metadata_path.read_text())
            for change in ({"git_commit": "b" * 40}, {"source_origin": "open"}, {"base_version": "99.0"},
                           {"flags": 0}, {"flags": 3}):
                with self.subTest(change=change):
                    metadata_path.write_text(json.dumps({**metadata, **change}))
                    expected = "dirty-worktree flag" if change == {"flags": 3} else "provenance"
                    with self.assertRaisesRegex(ValueError, expected):
                        package_artifacts.validate_firmware(firmware, row, COMMIT, "closed", True)
            metadata_path.write_text(json.dumps(metadata))
            other = matrix.select_builds(OPEN)[0]
            with self.assertRaisesRegex(ValueError, "explicit shipping"):
                package_artifacts.nuget_manifest(OPEN, other["id"], root / "staged", root / "bad.nuspec",
                                                 COMMIT, "hyperv.uefi", "main", True)
            (root / "staged/package/Bin/MSVM.fd").write_bytes(b"wrong")
            with self.assertRaisesRegex(ValueError, "differs"):
                package_artifacts.nuget_manifest(CLOSED, row["id"], root / "staged", root / "bad.nuspec",
                                                 COMMIT, "hyperv.uefi", "main", True)

    def test_dirty_pr_packs_warn_without_mutating_provenance(self) -> None:
        row = matrix.select_builds(CLOSED)[0]
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = fixture(root / "workspace", row, "closed", False)
            metadata_path = build / "FwVersion/FirmwareVersion.json"
            metadata = json.loads(metadata_path.read_text())
            metadata["flags"] = 1
            metadata_path.write_text(json.dumps(metadata))
            stage_artifacts.stage_build(root / "workspace", root / "staged", row, True)
            original = (root / "staged/firmware/FwVersion/FirmwareVersion.json").read_bytes()
            messages = io.StringIO()
            with redirect_stdout(messages):
                package_artifacts.nuget_manifest(CLOSED, row["id"], root / "staged", root / "test.nuspec",
                                                 COMMIT, "hyperv.uefi", "merge", False)
            self.assertIn("WARNING: PR test package", messages.getvalue())
            self.assertIn("dirty-worktree flag", messages.getvalue())
            self.assertEqual((root / "staged/firmware/FwVersion/FirmwareVersion.json").read_bytes(), original)
            self.assertTrue((root / "test.nuspec").is_file())
            with self.assertRaisesRegex(ValueError, "Official firmware cannot allow"):
                package_artifacts.validate_firmware(root / "staged/firmware", row, COMMIT, "closed", True,
                                                    allow_dirty=True)
            for change in ({"git_commit": "b" * 40}, {"source_origin": "open"}, {"flags": 3}):
                (root / "staged/firmware/FwVersion/FirmwareVersion.json").write_text(json.dumps({**metadata, **change}))
                with redirect_stdout(io.StringIO()), self.assertRaisesRegex(ValueError, "provenance"):
                    package_artifacts.nuget_manifest(CLOSED, row["id"], root / "staged", root / "bad.nuspec",
                                                     COMMIT, "hyperv.uefi", "merge", False)

    @unittest.skipUnless(os.name == "nt" and shutil.which("nuget"), "Requires Windows NuGet CLI")
    def test_real_nuget_pack_for_every_flavor_in_pr_and_official_modes(self) -> None:
        nuget = shutil.which("nuget")
        assert nuget is not None
        for row in matrix.select_builds(CLOSED):
            for official in (False, True):
                with self.subTest(flavor=row["id"], official=official), TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    build = fixture(root / "workspace", row, "closed", official)
                    if row["tool_chain"] == "GCC":
                        (build / "PDB/Module.pdb").unlink()
                    if not official:
                        metadata_path = build / "FwVersion/FirmwareVersion.json"
                        metadata = json.loads(metadata_path.read_text())
                        metadata["flags"] = 1
                        metadata_path.write_text(json.dumps(metadata))
                    with redirect_stdout(io.StringIO()):
                        stage_artifacts.stage_build(root / "workspace", root / "staged", row, True, ado=True)
                        package_artifacts.nuget_manifest(CLOSED, row["id"], root / "staged", root / "firmware.nuspec",
                                                         COMMIT, "hyperv.uefi", "main" if official else "merge", official)
                    result = subprocess.run(
                        [nuget, "pack", str(root / "firmware.nuspec"), "-OutputDirectory", str(root / "staged/packages"),
                         "-NoPackageAnalysis", "-NonInteractive"], capture_output=True, text=True,
                    )
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    packages = list((root / "staged/packages").glob("*.nupkg"))
                    self.assertEqual(len(packages), 1)
                    with zipfile.ZipFile(packages[0]) as archive:
                        names = archive.namelist()
                        legacy = f"Msvm{row['arch']}/{row['target']}_{row['tool_chain']}"
                        self.assertIn(f"{legacy}/FV/MSVM.fd", names)
                        self.assertIn(f"map/{legacy}/MAP/Module.map", names)
                        self.assertEqual(f"symbols/{legacy}/PDB/Module.pdb" in names, row["tool_chain"] != "GCC")
                        self.assertIn("BuildInfo.json", names)
                        metadata = json.loads(archive.read("FwVersion/FirmwareVersion.json"))
                        self.assertEqual(metadata["flags"], 2 if official else 1)
                        spec = ET.fromstring(archive.read(next(name for name in names if name.endswith(".nuspec"))))
                        self.assertEqual(spec.findtext("{*}metadata/{*}id"),
                                         matrix.package_name(row, "hyperv.uefi", official))

    @unittest.skipUnless(os.name == "nt" and shutil.which("nuget"), "Requires Windows NuGet CLI")
    def test_real_nuget_pack_matches_legacy_payload_paths(self) -> None:
        row = matrix.select_builds(CLOSED)[0]
        nuget = shutil.which("nuget")
        assert nuget is not None
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            fixture(root / "workspace", row, "closed", True)
            stage_artifacts.stage_build(root / "workspace", root / "staged", row, True)
            manifest = root / "new.nuspec"
            package_artifacts.nuget_manifest(CLOSED, row["id"], root / "staged", manifest,
                                             COMMIT, "hyperv.uefi", "main", True)
            legacy = ET.Element("package")
            metadata = ET.SubElement(legacy, "metadata")
            for key, value in {"id": "legacy", "version": "26.0", "authors": "Microsoft", "description": "Test"}.items():
                ET.SubElement(metadata, key).text = value
            files = ET.SubElement(legacy, "files")
            for src, target in (("FV/MSVM.fd", ""), ("PDB/*", "symbols"), ("MAP/*", "map")):
                ET.SubElement(files, "file", src=str(root / "workspace/Build" / "**" / src), target=target)
            legacy_path = root / "legacy.nuspec"
            ET.ElementTree(legacy).write(legacy_path, encoding="utf-8", xml_declaration=True)
            payloads: list[set[str]] = []
            for name, spec in (("legacy", legacy_path), ("staged/packages", manifest)):
                result = subprocess.run([nuget, "pack", str(spec), "-OutputDirectory", str(root / name),
                                         "-NoPackageAnalysis", "-NonInteractive"], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                with zipfile.ZipFile(next((root / name).glob("*.nupkg"))) as archive:
                    payloads.append({path for path in archive.namelist()
                                     if path.endswith((".fd", ".map", ".pdb"))})
            self.assertEqual(payloads[0], payloads[1])
            legacy_prefix = "MsvmX64/DEBUG_VS2022"
            self.assertEqual(payloads[1], {f"{legacy_prefix}/FV/MSVM.fd",
                                         f"symbols/{legacy_prefix}/PDB/Module.pdb",
                                         f"map/{legacy_prefix}/MAP/Module.map"})


class PublicationContractTests(unittest.TestCase):
    def test_every_build_job_stages_and_packs_its_final_artifact(self) -> None:
        job = load_yaml(".azuredevops/templates/build.yml")["jobs"][1]["${{ each host in parameters.hosts }}"][0]
        variables = job["variables"]
        self.assertEqual(variables["ob_nugetPublishing_enabled"], "${{ parameters.officialBuild }}")
        self.assertEqual(variables["ob_createvpack_enabled"], "${{ and(parameters.officialBuild, eq(host, 'windows')) }}")
        self.assertTrue(variables["ob_pipelineartifacts_enabled"])
        self.assertEqual(variables["ob_artifactBaseName"], "$(artifact_name)")
        self.assertEqual(variables["ob_artifacts_main_condition"], "succeededOrFailed()")
        for host in ("windows", "linux"):
            key = "${{ if eq(host, '" + host + "') }}"
            steps = next(branch[key] for branch in job["steps"] if key in branch)
            stage = next(i for i, step in enumerate(steps)
                         if step.get("template") == "/.azuredevops/templates/stage-artifacts.yml@self")
            pack = next(i for i, step in enumerate(steps)
                        if step.get("template") == "/.azuredevops/templates/pack.yml@self")
            self.assertLess(stage, pack)
            self.assertEqual(steps[pack]["parameters"]["officialBuild"], "${{ parameters.officialBuild }}")
        packing = load_yaml(".azuredevops/templates/pack.yml")["steps"]
        self.assertNotIn("shipping", packing[0]["script"])
        self.assertIn('--source "$(ob_outputDirectory)"', packing[0]["script"])
        self.assertEqual(packing[1]["task"], "NuGetCommand@2")
        self.assertEqual(packing[1]["inputs"]["command"], "pack")
        self.assertEqual(packing[1]["inputs"]["packDestination"], "$(ob_outputDirectory)/packages")
        staging = load_yaml(".azuredevops/templates/stage-artifacts.yml")["steps"]
        self.assertEqual(staging[0]["condition"], "succeeded()")
        self.assertEqual(staging[1]["condition"], "not(succeeded())")
        windows = next(branch["${{ if eq(host, 'windows') }}"] for branch in job["steps"]
                       if "${{ if eq(host, 'windows') }}" in branch)
        index = next(step for step in windows if step.get("displayName") == "Index Symbols")
        self.assertNotIn("shipping", index.get("condition", ""))
        self.assertTrue(index["inputs"]["IndexSources"])
        self.assertFalse(index["inputs"]["PublishSymbols"])
        symbol = windows[-1]["${{ if eq(parameters.officialBuild, true) }}"][0]
        self.assertTrue(symbol["inputs"]["PublishSymbols"])
        self.assertFalse(symbol["inputs"]["IndexSources"])
        linux = next(branch["${{ if eq(host, 'linux') }}"] for branch in job["steps"]
                     if "${{ if eq(host, 'linux') }}" in branch)
        self.assertIn("mono-devel", next(step["bash"] for step in linux if "bash" in step))

    def test_official_builds_require_checks_and_cannot_publish_on_prs(self) -> None:
        stages = load_yaml(".azuredevops/templates/platform.yml")["extends"]["parameters"]["stages"]
        self.assertEqual([stage["stage"] for stage in stages], ["SourceChecks", "Build"])
        build = stages[1]
        official = build["${{ if eq(parameters.oneBranchBase, 'v2/Microsoft.Official.yml') }}"]
        self.assertEqual(official["dependsOn"], ["SourceChecks"])
        self.assertEqual(build["${{ else }}"]["dependsOn"], [])
        gate = official["condition"]
        self.assertIn("succeeded()", gate)
        self.assertIn("'refs/heads/official/'", gate)
        self.assertIn("ne(variables['Build.Reason'], 'PullRequest')", gate)
        self.assertFalse((ROOT / ".azuredevops/templates/publish.yml").exists())
        template = load_yaml(".azuredevops/templates/build.yml")
        self.assertFalse(next(parameter["default"] for parameter in template["parameters"]
                              if parameter["name"] == "officialBuild"))
        job = template["jobs"][1]["${{ each host in parameters.hosts }}"][0]
        for step in job["steps"]:
            self.assertNotEqual(step.get("task"), "DownloadPipelineArtifact@2")
        source = load_yaml(".azuredevops/templates/source-checks.yml")["jobs"][0]
        self.assertEqual(source["variables"]["ob_artifactBaseName"], "drop_Build_SourceChecks")

    def test_github_release_and_failure_artifact_gates(self) -> None:
        workflow = load_yaml(".github/workflows/platform-ci.yml")
        release = workflow["jobs"]["release"]
        self.assertEqual(release["if"], "github.event_name == 'push' && github.ref == 'refs/heads/main'")
        self.assertEqual(release["needs"], ["source-checks", "build"])
        self.assertEqual(release["permissions"]["contents"], "write")
        self.assertFalse(release["concurrency"]["cancel-in-progress"])
        stage = workflow["jobs"]["build"]["steps"][-1]
        self.assertEqual(stage["if"], "always()")
        self.assertEqual(stage["with"]["build-succeeded"], "${{ steps.firmware.outcome == 'success' }}")
        upload = load_yaml(".github/actions/stage-artifacts/action.yml")["runs"]["steps"]
        self.assertEqual(upload[1]["with"]["if-no-files-found"], "error")
        self.assertEqual(upload[2]["if"], "always()")


if __name__ == "__main__":
    unittest.main()

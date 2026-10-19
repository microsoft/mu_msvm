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

    def test_nuget_identity_and_payload_for_all_shipping_flavors(self) -> None:
        for row in matrix.select_builds(CLOSED):
            if row["shipping"] != "true":
                continue
            for official in (True, False):
                with self.subTest(flavor=row["id"], official=official), TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    fixture(root / "workspace", row, "closed", official)
                    stage_artifacts.stage_build(root / "workspace", root / "staged", row, True)
                    output = root / "package.nuspec"
                    package_artifacts.nuget_manifest(CLOSED, row["id"], root / "staged", output,
                                                     COMMIT, "hyperv.uefi", "main" if official else "merge", official)
                    manifest = ET.parse(output)
                    prefix, suffix = ("", "") if official else ("PR.", ".Test")
                    self.assertEqual(manifest.findtext("metadata/id"),
                                     f"hyperv.uefi.mscoreuefi.{prefix}{row['arch']}.{row['target']}{suffix}")
                    legacy = f"Msvm{row['arch']}/{row['target']}_{row['tool_chain']}"
                    self.assertEqual({file.get("target") for file in manifest.findall("files/file")},
                                     {"", f"{legacy}/FV", f"symbols/{legacy}/PDB", f"map/{legacy}/MAP", "FwVersion"})
                    self.assertTrue(all(Path(file.attrib["src"]).is_absolute() for file in manifest.findall("files/file")))

    def test_rejects_nonshipping_or_wrong_provenance_packages(self) -> None:
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
                    with self.assertRaisesRegex(ValueError, "provenance"):
                        package_artifacts.validate_firmware(firmware, row, COMMIT, "closed", True)
            metadata_path.write_text(json.dumps(metadata))
            for other in matrix.select_builds(CLOSED)[3:]:
                with self.assertRaisesRegex(ValueError, "Only shipping"):
                    package_artifacts.nuget_manifest(CLOSED, other["id"], root / "staged", root / "bad.nuspec",
                                                     COMMIT, "hyperv.uefi", "main", True)
            (root / "staged/package/Bin/MSVM.fd").write_bytes(b"wrong")
            with self.assertRaisesRegex(ValueError, "differs"):
                package_artifacts.nuget_manifest(CLOSED, row["id"], root / "staged", root / "bad.nuspec",
                                                 COMMIT, "hyperv.uefi", "main", True)

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
            (root / "staged/build.manifest").write_text("Provider metadata must not be reused")
            package_artifacts.nuget_manifest(CLOSED, row["id"], root / "staged", manifest,
                                             COMMIT, "hyperv.uefi", "main", True, root / "publication")
            self.assertFalse((root / "publication/build.manifest").exists())
            self.assertTrue((root / "publication/Firmware Binary File DEBUG_X64/MSVM.fd").is_file())
            self.assertTrue((root / "publication/Build/MsvmX64/DEBUG_VS2022/Bin/Dsdt.aml").is_file())
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
            for name, spec in (("legacy", legacy_path), ("publication/packages", manifest)):
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
    def test_build_jobs_upload_but_cannot_publish_packages(self) -> None:
        job = load_yaml(".azuredevops/templates/build.yml")["jobs"][1]["${{ each host in parameters.hosts }}"][0]
        variables = job["variables"]
        self.assertFalse(variables["ob_nugetPublishing_enabled"])
        self.assertFalse(variables["ob_createvpack_enabled"])
        self.assertTrue(variables["ob_pipelineartifacts_enabled"])
        self.assertEqual(variables["ob_artifactBaseName"], "$(build_artifact_name)")
        for host in ("windows", "linux"):
            key = "${{ if eq(host, '" + host + "') }}"
            steps = next(branch[key] for branch in job["steps"] if key in branch)
            self.assertEqual(steps[-1]["template"], "/.azuredevops/templates/stage-artifacts.yml@self")
        windows = next(branch["${{ if eq(host, 'windows') }}"] for branch in job["steps"]
                       if "${{ if eq(host, 'windows') }}" in branch)
        index = next(step for step in windows if step.get("displayName") == "Index Shipping Symbols")
        self.assertEqual(index["condition"], "and(succeeded(), eq(variables['shipping'], 'true'))")
        self.assertTrue(index["inputs"]["IndexSources"])
        self.assertFalse(index["inputs"]["PublishSymbols"])

    def test_packages_wait_for_all_checks_and_official_branch(self) -> None:
        stages = load_yaml(".azuredevops/templates/platform.yml")["extends"]["parameters"]["stages"]
        packages = next(stage for stage in stages if stage["stage"] == "Packages")
        self.assertEqual(packages["dependsOn"], ["SourceChecks", "Build"])
        gate = packages["${{ if eq(parameters.oneBranchBase, 'v2/Microsoft.Official.yml') }}"]["condition"]
        self.assertIn("succeeded()", gate)
        self.assertIn("'refs/heads/official/'", gate)
        self.assertIn("ne(variables['Build.Reason'], 'PullRequest')", gate)
        template = load_yaml(".azuredevops/templates/publish.yml")
        self.assertFalse(template["parameters"][0]["default"])
        self.assertIn("--shipping-only", template["jobs"][0]["steps"][1]["pwsh"])
        job = template["jobs"][1]
        self.assertEqual(job["pool"]["type"], "windows")
        self.assertEqual(job["variables"]["ob_artifactBaseName"], "$(artifact_name)")
        download = next(step for step in job["steps"] if step.get("task") == "DownloadPipelineArtifact@2")
        self.assertEqual(download["inputs"]["artifactName"], "$(build_artifact_name)")
        source = load_yaml(".azuredevops/templates/source-checks.yml")["jobs"][0]
        self.assertEqual(source["variables"]["ob_artifactBaseName"], "drop_Build_SourceChecks")
        for variable in ("ob_createvpack_enabled", "ob_nugetPublishing_enabled"):
            self.assertEqual(job["variables"][variable], "${{ parameters.officialBuild }}")
        symbol = job["steps"][-1]["${{ if eq(parameters.officialBuild, true) }}"][0]
        self.assertTrue(symbol["inputs"]["PublishSymbols"])
        self.assertFalse(symbol["inputs"]["IndexSources"])
        self.assertNotIn("continueOnError", symbol)

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
